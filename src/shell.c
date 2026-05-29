#include "shell.h"

#include "console.h"
#include "format.h"
#include "initrd.h"
#include "keyboard.h"
#include "memory.h"
#include "pit.h"
#include "power.h"
#include "abi/linux.h"
#include "string.h"
#include "task.h"

#define SHELL_LINE_MAX 256
#define SHELL_ARG_MAX  16
#define SHELL_FASTFETCH_CONFIG "/root/.config/fastfetch/config.jsonc"
#define SHELL_FETCH_CONFIG     "/root/.config/fastfetch/minimal.jsonc"

/* Color scheme - easy on the eyes */
#define COLOR_PROMPT_USER   CONSOLE_LIGHT_CYAN
#define COLOR_PROMPT_HOST   CONSOLE_LIGHT_GREEN
#define COLOR_PROMPT_PATH   CONSOLE_LIGHT_BLUE
#define COLOR_PROMPT_ARROW  CONSOLE_WHITE
#define COLOR_ERROR         CONSOLE_LIGHT_RED
#define COLOR_SUCCESS       CONSOLE_LIGHT_GREEN
#define COLOR_DEFAULT       CONSOLE_WHITE

static task_state_t shell_parent_state;

static void shell_parent_init(void) {
    memset(&shell_parent_state, 0, sizeof(shell_parent_state));
    shell_parent_state.cwd = vfs_root();
    strcpy(shell_parent_state.cwd_path, "/");

    for (int fd = 0; fd < 3; ++fd) {
        shell_parent_state.fds[fd].used = true;
        shell_parent_state.fds[fd].kind = TASK_FD_CONSOLE;
        shell_parent_state.fds[fd].flags = (fd == 0) ? LINUX_O_RDONLY : LINUX_O_WRONLY;
        shell_parent_state.fds[fd].offset = 0;
        shell_parent_state.fds[fd].node = NULL;
        shell_parent_state.fds[fd].socket = NULL;
    }
}

static int shell_tokenize(char *line, char **argv, int max_args) {
    int argc = 0;

    while (*line && argc < max_args) {
        while (isspace(*line)) {
            *line++ = '\0';
        }
        if (!*line) {
            break;
        }
        argv[argc++] = line;
        while (*line && !isspace(*line)) {
            line++;
        }
    }

    return argc;
}

static void shell_help(void) {
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write("┌─ Available Commands ─────────────────────┐\n");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("help");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("              show this command list\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("cd <dir>");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("          change working directory\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("clear");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("             wipe the screen\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("mem");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("               print memory and heap stats\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("ticks");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("             show timer state\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("hexdump <path>");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("    hex view any initrd file\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("run <path> ...");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("    load and execute an ELF32 user binary\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("shutdown");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("          power the machine off\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("reboot");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("            reset the machine\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("suspend");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("           enter ACPI S1 when available\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("fastfetch");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("         show the full machine summary\n");
    console_write("  ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("fetch");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("             show the compact machine summary\n");
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write("└──────────────────────────────────────────┘\n");
}

static void shell_mem(void) {
    memory_stats_t stats = memory_stats();
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write("┌─ Memory Statistics ──────────────────────┐\n");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("  Total:     ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_printf("%u KiB", stats.total_bytes / 1024u);
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("\n  Heap:      ");
    console_set_color(COLOR_PROMPT_PATH, CONSOLE_BLACK);
    console_printf("%x - %x", stats.heap_start, stats.heap_end);
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("\n  Used:      ");
    console_set_color(COLOR_ERROR, CONSOLE_BLACK);
    console_printf("%u bytes", stats.heap_used);
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("\n  Free:      ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_printf("%u bytes", stats.heap_free);
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("\n  Initrd:    ");
    console_set_color(COLOR_PROMPT_HOST, CONSOLE_BLACK);
    console_printf("%u bytes", initrd_module_size());
    console_write("\n");
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write("└────────────────────────────────────────┘\n");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
}

static void shell_ticks(void) {
    console_printf("ticks: %u at %u Hz\n", pit_ticks(), pit_frequency());
}

static void shell_hexdump(const char *path) {
    const initrd_file_t *file = initrd_find(path);
    if (!file) {
        console_set_color(COLOR_ERROR, CONSOLE_BLACK);
        console_printf("hexdump: %s: not found\n", path);
        console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
        return;
    }
    console_hexdump(file->data, file->size, 0);
}

static void shell_run_binary(int argc, char **argv) {
    int exit_code = task_run_argv_alongside(NULL, argv[0], argc, (const char *const *)argv);
    if (exit_code == -1) {
        console_set_color(COLOR_ERROR, CONSOLE_BLACK);
        console_printf("run: %s failed to load or execute\n", argv[0]);
        console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
        return;
    }
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_printf("[task] exited with code %d\n", exit_code);
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
}

static bool shell_argv_has_config(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) {
            return true;
        }
        if (strncmp(argv[i], "--config=", 9) == 0) {
            return true;
        }
    }
    return false;
}

static bool shell_run_fastfetch(int argc, char **argv, const char *binary_path, const char *config_path) {
    const char *task_argv[SHELL_ARG_MAX + 2];
    int task_argc = 0;
    int exit_code;
    bool has_config;

    if (argc <= 0) {
        return false;
    }

    has_config = shell_argv_has_config(argc, argv);

    task_argv[task_argc++] = binary_path;
    if (!has_config) {
        task_argv[task_argc++] = "--config";
        task_argv[task_argc++] = config_path;
    }

    for (int i = 1; i < argc; ++i) {
        task_argv[task_argc++] = argv[i];
    }

    exit_code = task_run_argv_alongside(NULL, binary_path, task_argc, task_argv);
    if (exit_code < 0) {
        console_set_color(COLOR_ERROR, CONSOLE_BLACK);
        console_printf("%s failed to load or execute\n", argv[0]);
        console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
        return true;
    }

    console_printf("[task] exited with code %d\n", exit_code);
    return true;
}

static bool shell_try_user_program(int argc, char **argv) {
    char resolved[VFS_PATH_MAX];
    const char *task_argv[SHELL_ARG_MAX];
    int exit_code;

    if (argc <= 0) {
        return false;
    }

    if (argv[0][0] == '/') {
        strncpy(resolved, argv[0], sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = '\0';
    } else {
        snprintf(resolved, sizeof(resolved), "/bin/%s", argv[0]);
    }

    task_argv[0] = resolved;
    for (int i = 1; i < argc; ++i) {
        task_argv[i] = argv[i];
    }

    exit_code = task_run_argv_alongside(&shell_parent_state, resolved, argc, task_argv);
    if (exit_code < 0) {
        return false;
    }

    console_printf("[task] exited with code %d\n", exit_code);
    return true;
}

static void shell_cd(const char *path) {
    const vfs_node_t *node;

    if (!path || !*path) {
        return;
    }

    node = vfs_resolve(shell_parent_state.cwd, path);
    if (!node || !vfs_is_dir(node)) {
        console_printf("cd: %s: not a directory\n", path);
        return;
    }

    shell_parent_state.cwd = node;
    strncpy(shell_parent_state.cwd_path, vfs_path(node), sizeof(shell_parent_state.cwd_path) - 1);
    shell_parent_state.cwd_path[sizeof(shell_parent_state.cwd_path) - 1] = '\0';
}

static void shell_shutdown(void) {
    console_write("Powering down.\n");
    if (!power_shutdown()) {
        console_write("Shutdown did not complete on this machine.\n");
    }
}

static void shell_reboot(void) {
    console_write("Rebooting.\n");
    power_reboot();
}

static void shell_suspend(void) {
    console_write("Suspending. Wake it with input.\n");
    if (power_suspend()) {
        console_write("Resumed.\n");
    } else {
        console_write("Resume path returned through the fallback idle wait.\n");
    }
}

static void shell_execute(char *line) {
    char *argv[SHELL_ARG_MAX];
    int argc = shell_tokenize(line, argv, SHELL_ARG_MAX);

    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "help") == 0) {
        shell_help();
    } else if (strcmp(argv[0], "clear") == 0) {
        console_clear();
    } else if (strcmp(argv[0], "cd") == 0) {
        shell_cd(argc >= 2 ? argv[1] : "/");
    } else if (strcmp(argv[0], "mem") == 0) {
        shell_mem();
    } else if (strcmp(argv[0], "ticks") == 0) {
        shell_ticks();
    } else if (strcmp(argv[0], "hexdump") == 0 && argc >= 2) {
        shell_hexdump(argv[1]);
    } else if (strcmp(argv[0], "run") == 0 && argc >= 2) {
        shell_run_binary(argc - 1, argv + 1);
    } else if (strcmp(argv[0], "shutdown") == 0) {
        shell_shutdown();
    } else if (strcmp(argv[0], "reboot") == 0) {
        shell_reboot();
    } else if (strcmp(argv[0], "suspend") == 0) {
        shell_suspend();
    } else if (strcmp(argv[0], "fastfetch") == 0) {
        shell_run_fastfetch(argc, argv, "/bin/fastfetch", SHELL_FASTFETCH_CONFIG);
    } else if (strcmp(argv[0], "fetch") == 0) {
        shell_run_fastfetch(argc, argv, "/bin/fetch", SHELL_FETCH_CONFIG);
    } else if (!shell_try_user_program(argc, argv)) {
        console_set_color(COLOR_ERROR, CONSOLE_BLACK);
        console_printf("%s: unknown command\n", argv[0]);
        console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    } else {
    }
}

static void shell_print_prompt(void) {
    /* Print colored prompt: user@host:path> */
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write("root");
    console_set_color(COLOR_PROMPT_ARROW, CONSOLE_BLACK);
    console_write("@");
    console_set_color(COLOR_PROMPT_HOST, CONSOLE_BLACK);
    console_write("openhobby");
    console_set_color(COLOR_PROMPT_ARROW, CONSOLE_BLACK);
    console_write(":");
    console_set_color(COLOR_PROMPT_PATH, CONSOLE_BLACK);
    console_write(shell_parent_state.cwd_path);
    console_set_color(COLOR_PROMPT_ARROW, CONSOLE_BLACK);
    console_write("$ ");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
}

static void shell_welcome(void) {
    console_clear();
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write("╔══════════════════════════════════════════╗\n");
    console_write("║  ");
    console_set_color(CONSOLE_WHITE, CONSOLE_BLACK);
    console_write("OpenHobbyOS");
    console_set_color(COLOR_PROMPT_USER, CONSOLE_BLACK);
    console_write(" - The Small Workspace              ║\n");
    console_write("╚══════════════════════════════════════════╝\n\n");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write("Welcome! Type ");
    console_set_color(COLOR_SUCCESS, CONSOLE_BLACK);
    console_write("'help'");
    console_set_color(COLOR_DEFAULT, CONSOLE_BLACK);
    console_write(" for available commands.\n\n");
}

void shell_run(void) {
    char line[SHELL_LINE_MAX];

    shell_parent_init();
    shell_welcome();

    for (;;) {
        shell_print_prompt();
        keyboard_readline(line, sizeof(line));
        shell_execute(line);
    }
}
