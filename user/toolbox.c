#include "runtime.h"
#include "syscall.h"

#define TOOLBOX_BUFFER_SIZE 512
#define TOOLBOX_DIRENT_SIZE 1024
#define TOOLBOX_CHILDREN_MAX 8
#define TOOLBOX_ARG_MAX 32

static void tb_resolve_program_path(char *resolved, unsigned int size, const char *path) {
    if (size == 0) {
        return;
    }

    if (path && path[0] == '/') {
        unsigned int index = 0;
        while (path[index] && index + 1u < size) {
            resolved[index] = path[index];
            index++;
        }
        resolved[index] = '\0';
        return;
    }

    if (!path) {
        resolved[0] = '\0';
        return;
    }

    u_strcpy(resolved, "/bin/");
    {
        unsigned int used = u_strlen(resolved);
        unsigned int index = 0;
        while (path[index] && used + 1u < size) {
            resolved[used++] = path[index++];
        }
        resolved[used] = '\0';
    }
}

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

static int tb_cmd_clear(void) {
    static const char clear_sequence[] = "\033[2J\033[H";

    if (tb_write_all(1, clear_sequence, sizeof(clear_sequence) - 1u) < 0) {
        tb_print_error("clear", NULL, -1);
        return 1;
    }

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

static int tb_cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        u_puts("mkdir: usage: mkdir <path> [path...]\n");
        return 1;
    }

    int failed = 0;
    for (int i = 1; i < argc; ++i) {
        int rc = sys_mkdir(argv[i], 0777);
        if (rc < 0) {
            tb_print_error("mkdir", argv[i], rc);
            failed = 1;
        }
    }
    return failed;
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

static int tb_cmd_poweroff(void) {
    u_puts("poweroff: shutting down...\n");
    sys_shutdown();
    u_puts("poweroff: shutdown failed\n");
    return 1;
}

static int tb_cmd_reboot(void) {
    u_puts("reboot: rebooting...\n");
    sys_reboot();
    return 1;
}

static int tb_cmd_suspend(void) {
    u_puts("suspend: suspending...\n");
    int rc = sys_suspend();
    if (rc < 0) {
        tb_print_error("suspend", NULL, rc);
        return 1;
    }
    u_puts("suspend: wake\n");
    return 0;
}

static int tb_cmd_yield(int argc, char **argv) {
    int ok = 0;
    unsigned int rounds = 1;

    if (argc > 2) {
        u_puts("yield: usage: yield [count]\n");
        return 1;
    }

    if (argc == 2) {
        rounds = u_parse_uint(argv[1], &ok);
        if (!ok || rounds == 0) {
            u_puts("yield: count must be a positive decimal\n");
            return 1;
        }
    }

    for (unsigned int i = 0; i < rounds; ++i) {
        int rc = sys_sched_yield();
        if (rc < 0) {
            tb_print_error("yield", NULL, rc);
            return 1;
        }
    }

    return 0;
}

static int tb_cmd_parallel(int argc, char **argv) {
    int child_pids[TOOLBOX_CHILDREN_MAX];
    char resolved_paths[TOOLBOX_CHILDREN_MAX][256];
    int child_count = 0;
    int segment_start = 1;
    int overall_status = 0;

    if (argc < 2) {
        u_puts("parallel: usage: parallel <cmd> [args...] [-- <cmd> [args...]]\n");
        return 1;
    }

    for (int index = 1; index <= argc; ++index) {
        int is_break = (index == argc) || u_strcmp(argv[index], "--") == 0;

        if (!is_break) {
            continue;
        }

        if (segment_start >= index) {
            u_puts("parallel: empty command group around '--'\n");
            return 1;
        }

        if (child_count >= TOOLBOX_CHILDREN_MAX) {
            u_puts("parallel: too many child commands\n");
            return 1;
        }

        {
            const char *child_argv[TOOLBOX_ARG_MAX];
            int child_argc = 0;
            int pid;

            tb_resolve_program_path(resolved_paths[child_count], sizeof(resolved_paths[child_count]), argv[segment_start]);

            child_argv[child_argc++] = resolved_paths[child_count];
            for (int arg = segment_start + 1; arg < index; ++arg) {
                if (child_argc + 1 >= TOOLBOX_ARG_MAX) {
                    u_puts("parallel: one child command is too long\n");
                    return 1;
                }
                child_argv[child_argc++] = argv[arg];
            }
            child_argv[child_argc] = 0;

            pid = sys_spawn(resolved_paths[child_count], child_argv);
            if (pid < 0) {
                tb_print_error("parallel", resolved_paths[child_count], pid);
                return 1;
            }

            child_pids[child_count] = pid;
            {
                char line[320];
                unsigned int used = 0;

                used = u_append_text(line, sizeof(line), used, "parallel: spawned ");
                used = u_append_text(line, sizeof(line), used, resolved_paths[child_count]);
                used = u_append_text(line, sizeof(line), used, " as pid ");
                used = u_append_int(line, sizeof(line), used, pid);
                used = u_append_text(line, sizeof(line), used, "\n");
                u_write_buffer(line, used);
            }
            child_count++;
        }

        segment_start = index + 1;
    }

    for (int index = 0; index < child_count; ++index) {
        int status = 0;
        int waited = sys_waitpid(child_pids[index], &status, 0);

        if (waited < 0) {
            tb_print_error("parallel wait", NULL, waited);
            overall_status = 1;
            continue;
        }

        {
            char line[96];
            unsigned int used = 0;

            used = u_append_text(line, sizeof(line), used, "parallel: pid ");
            used = u_append_int(line, sizeof(line), used, waited);
            used = u_append_text(line, sizeof(line), used, " exited with status ");
            used = u_append_int(line, sizeof(line), used, (status >> 8) & 0xFF);
            used = u_append_text(line, sizeof(line), used, "\n");
            u_write_buffer(line, used);
        }
        if (((status >> 8) & 0xFF) != 0) {
            overall_status = 1;
        }
    }

    return overall_status;
}

static int tb_show_help(void) {
    u_puts("toolbox commands: ls cat stat pwd env id echo clear sleep mkdir uname parallel yield poweroff reboot suspend help\n");
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
    if (u_strcmp(command, "clear") == 0) {
        return tb_cmd_clear();
    }
    if (u_strcmp(command, "sleep") == 0) {
        return tb_cmd_sleep(argc, argv);
    }
    if (u_strcmp(command, "mkdir") == 0) {
        return tb_cmd_mkdir(argc, argv);
    }
    if (u_strcmp(command, "uname") == 0) {
        return tb_cmd_uname();
    }
    if (u_strcmp(command, "parallel") == 0) {
        return tb_cmd_parallel(argc, argv);
    }
    if (u_strcmp(command, "yield") == 0) {
        return tb_cmd_yield(argc, argv);
    }
    if (u_strcmp(command, "poweroff") == 0) {
        return tb_cmd_poweroff();
    }
    if (u_strcmp(command, "reboot") == 0) {
        return tb_cmd_reboot();
    }
    if (u_strcmp(command, "suspend") == 0) {
        return tb_cmd_suspend();
    }
    if (u_strcmp(command, "help") == 0) {
        return tb_show_help();
    }
    u_puts("toolbox: unknown applet: ");
    u_puts(command);
    u_puts("\n");
    return 1;
}
