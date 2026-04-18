#include "runtime.h"
#include "syscall.h"

#define TOOLBOX_BUFFER_SIZE 512
#define TOOLBOX_DIRENT_SIZE 1024

static void tb_print_error(const char *command, const char *target, int code) {
    u_puts(command);
    if (target && *target) {
        u_puts(": ");
        u_puts(target);
    }
    u_puts(": error ");
    u_put_int(code);
    u_puts("\n");
}

static int tb_write_all(int fd, const void *buffer, unsigned int length) {
    const char *cursor = (const char *)buffer;
    unsigned int remaining = length;

    while (remaining) {
        int written = sys_write(fd, cursor, remaining);
        if (written < 0) {
            return written;
        }
        if (written == 0) {
            return -1;
        }
        cursor += written;
        remaining -= (unsigned int)written;
    }

    return 0;
}

static int tb_cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            u_puts(" ");
        }
        u_puts(argv[i]);
    }
    u_puts("\n");
    return 0;
}

static int tb_cmd_pwd(void) {
    char cwd[256];
    int rc = sys_getcwd(cwd, sizeof(cwd));

    if (rc < 0) {
        tb_print_error("pwd", NULL, rc);
        return 1;
    }

    u_puts(cwd);
    u_puts("\n");
    return 0;
}

static int tb_cmd_env(char **envp) {
    for (char **entry = envp; entry && *entry; ++entry) {
        u_puts(*entry);
        u_puts("\n");
    }
    return 0;
}

static int tb_cmd_id(void) {
    u_puts("uid=");
    u_put_uint((unsigned int)sys_getuid32());
    u_puts(" gid=");
    u_put_uint((unsigned int)sys_getgid32());
    u_puts(" euid=");
    u_put_uint((unsigned int)sys_geteuid32());
    u_puts(" egid=");
    u_put_uint((unsigned int)sys_getegid32());
    u_puts("\n");
    return 0;
}

static int tb_cat_one(const char *path) {
    char buffer[TOOLBOX_BUFFER_SIZE];
    int fd = sys_open(path, LINUX_O_RDONLY, 0);

    if (fd < 0) {
        tb_print_error("cat", path, fd);
        return 1;
    }

    for (;;) {
        int read_count = sys_read(fd, buffer, sizeof(buffer));
        if (read_count < 0) {
            sys_close(fd);
            tb_print_error("cat", path, read_count);
            return 1;
        }
        if (read_count == 0) {
            break;
        }
        if (tb_write_all(1, buffer, (unsigned int)read_count) < 0) {
            sys_close(fd);
            tb_print_error("cat", path, -1);
            return 1;
        }
    }

    sys_close(fd);
    return 0;
}

static int tb_cmd_cat(int argc, char **argv) {
    int failed = 0;

    if (argc < 2) {
        u_puts("cat: missing path\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (tb_cat_one(argv[i]) != 0) {
            failed = 1;
        }
    }

    return failed;
}

static int tb_stat_one(const char *path) {
    struct linux_stat64 stat;
    int rc = sys_stat64(path, &stat);

    if (rc < 0) {
        tb_print_error("stat", path, rc);
        return 1;
    }

    u_print_stat(path, &stat);
    return 0;
}

static int tb_cmd_stat(int argc, char **argv) {
    int failed = 0;

    if (argc < 2) {
        u_puts("stat: missing path\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (tb_stat_one(argv[i]) != 0) {
            failed = 1;
        }
    }

    return failed;
}

static int tb_print_dir_entries(const char *path, int many) {
    char buffer[TOOLBOX_DIRENT_SIZE];
    int fd = sys_open(path, LINUX_O_RDONLY | LINUX_O_DIRECTORY, 0);

    if (fd < 0) {
        tb_print_error("ls", path, fd);
        return 1;
    }

    if (many) {
        u_puts(path);
        u_puts(":\n");
    }

    for (;;) {
        int size = sys_getdents64(fd, buffer, sizeof(buffer));
        unsigned int offset = 0;

        if (size < 0) {
            sys_close(fd);
            tb_print_error("ls", path, size);
            return 1;
        }
        if (size == 0) {
            break;
        }

        while (offset < (unsigned int)size) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + offset);
            if (entry->d_name[0] != '\0') {
                u_puts(entry->d_name);
                if (entry->d_type == LINUX_DT_DIR) {
                    u_puts("/");
                }
                u_puts("\n");
            }
            if (entry->d_reclen == 0) {
                sys_close(fd);
                u_puts("ls: kernel returned a broken dirent\n");
                return 1;
            }
            offset += entry->d_reclen;
        }
    }

    sys_close(fd);
    return 0;
}

static int tb_cmd_ls(int argc, char **argv) {
    int failed = 0;
    int many = argc > 2;

    if (argc == 1) {
        return tb_print_dir_entries(".", 0);
    }

    for (int i = 1; i < argc; ++i) {
        if (tb_print_dir_entries(argv[i], many) != 0) {
            failed = 1;
        }
        if (many && i + 1 < argc) {
            u_puts("\n");
        }
    }

    return failed;
}

static int tb_cmd_sleep(int argc, char **argv) {
    struct linux_timespec req;
    int ok = 0;
    unsigned int seconds;
    int rc;

    if (argc != 2) {
        u_puts("sleep: usage: sleep <seconds>\n");
        return 1;
    }

    seconds = u_parse_uint(argv[1], &ok);
    if (!ok) {
        u_puts("sleep: seconds must be decimal\n");
        return 1;
    }

    req.tv_sec = (int)seconds;
    req.tv_nsec = 0;
    rc = sys_nanosleep(&req, 0);
    if (rc < 0) {
        tb_print_error("sleep", argv[1], rc);
        return 1;
    }

    return 0;
}

static int tb_cmd_uname(void) {
    struct linux_utsname name;

    if (sys_uname(&name) < 0) {
        u_puts("uname: syscall failed\n");
        return 1;
    }

    u_print_uname(&name);
    return 0;
}

static int tb_show_help(void) {
    u_puts("toolbox commands: ls cat stat pwd env id echo sleep uname\n");
    return 0;
}

int main(int argc, char **argv, char **envp) {
    const char *command;

    if (argc <= 0 || !argv || !argv[0]) {
        return 1;
    }

    command = u_basename(argv[0]);
    if (u_strcmp(command, "toolbox") == 0) {
        if (argc == 1) {
            return tb_show_help();
        }
        command = argv[1];
        argv++;
        argc--;
    }

    if (u_strcmp(command, "ls") == 0) {
        return tb_cmd_ls(argc, argv);
    }
    if (u_strcmp(command, "cat") == 0) {
        return tb_cmd_cat(argc, argv);
    }
    if (u_strcmp(command, "stat") == 0) {
        return tb_cmd_stat(argc, argv);
    }
    if (u_strcmp(command, "pwd") == 0) {
        return tb_cmd_pwd();
    }
    if (u_strcmp(command, "env") == 0) {
        return tb_cmd_env(envp);
    }
    if (u_strcmp(command, "id") == 0) {
        return tb_cmd_id();
    }
    if (u_strcmp(command, "echo") == 0) {
        return tb_cmd_echo(argc, argv);
    }
    if (u_strcmp(command, "sleep") == 0) {
        return tb_cmd_sleep(argc, argv);
    }
    if (u_strcmp(command, "uname") == 0) {
        return tb_cmd_uname();
    }

    u_puts("toolbox: unknown applet: ");
    u_puts(command);
    u_puts("\n");
    return 1;
}
