#include "runtime.h"
#include "syscall.h"

#define SH_ARG_MAX 32
#define SH_LINE_MAX 1024

static void sh_puterr(const char *text) {
    sys_write(2, text, u_strlen(text));
}

static int sh_parse_command(char *line, const char **argv) {
    int argc = 0;
    char *read_cursor = line;
    char *write_cursor = line;
    char quote = '\0';

    while (*read_cursor != '\0') {
        while (*read_cursor == ' ' || *read_cursor == '\t' || *read_cursor == '\n') {
            read_cursor++;
        }
        if (*read_cursor == '\0') {
            break;
        }
        if (argc + 1 >= SH_ARG_MAX) {
            return -1;
        }

        argv[argc++] = write_cursor;
        quote = '\0';

        while (*read_cursor != '\0') {
            char ch = *read_cursor++;

            if (quote != '\0') {
                if (ch == quote) {
                    quote = '\0';
                    continue;
                }
                if (ch == '\\' && quote == '"' && (*read_cursor == '"' || *read_cursor == '\\')) {
                    ch = *read_cursor++;
                }
                *write_cursor++ = ch;
                continue;
            }

            if (ch == '"' || ch == '\'') {
                quote = ch;
                continue;
            }
            if (ch == ' ' || ch == '\t' || ch == '\n') {
                break;
            }
            if (ch == '\\' && *read_cursor != '\0') {
                ch = *read_cursor++;
            }
            *write_cursor++ = ch;
        }

        if (quote != '\0') {
            return -1;
        }

        *write_cursor++ = '\0';
    }

    argv[argc] = 0;
    return argc;
}

int main(int argc, char **argv, char **envp) {
    char command_buffer[SH_LINE_MAX];
    const char *child_argv[SH_ARG_MAX];
    int child_argc;
    int child_pid;
    int child_status = 0;

    (void)envp;

    if (argc != 3 || u_strcmp(argv[1], "-c") != 0) {
        sh_puterr("sh: only 'sh -c <command>' is supported here\n");
        return 2;
    }

    if (u_strlen(argv[2]) >= sizeof(command_buffer)) {
        sh_puterr("sh: command is too long\n");
        return 2;
    }

    u_strcpy(command_buffer, argv[2]);
    child_argc = sh_parse_command(command_buffer, child_argv);
    if (child_argc <= 0) {
        sh_puterr("sh: failed to parse command line\n");
        return 2;
    }

    child_pid = sys_spawn(child_argv[0], child_argv);
    if (child_pid < 0) {
        sh_puterr("sh: failed to spawn child command\n");
        return 127;
    }

    if (sys_waitpid(child_pid, &child_status, 0) < 0) {
        sh_puterr("sh: failed to wait for child command\n");
        return 127;
    }

    return child_status;
}
