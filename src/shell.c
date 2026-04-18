#include "shell.h"

#include "console.h"
#include "format.h"
#include "initrd.h"
#include "keyboard.h"
#include "memory.h"
#include "pit.h"
#include "power.h"
#include "string.h"
#include "task.h"

#define SHELL_LINE_MAX 256
#define SHELL_ARG_MAX  16
#define SHELL_FASTFETCH_CONFIG "/root/.config/fastfetch/config.jsonc"
#define SHELL_FETCH_CONFIG     "/root/.config/fastfetch/minimal.jsonc"

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
    console_write(
        "help              show this command list\n"
        "clear             wipe the screen\n"
        "mem               print memory and heap stats\n"
        "ticks             show timer state\n"
        "hexdump <path>    hex view any initrd file\n"
        "run <path> ...    load and execute an ELF32 user binary\n"
        "shutdown          power the machine off\n"
        "reboot            reset the machine\n"
        "suspend           enter ACPI S1 when available, then wake back up\n"
        "user commands     uname ls cat stat pwd env id echo sleep hello\n"
        "fastfetch         show the full machine summary\n"
        "fetch             show the compact machine summary\n"
        "extras            Xvfb\n"
    );
}

static void shell_mem(void) {
    memory_stats_t stats = memory_stats();
    console_printf("total memory: %u KiB\n", stats.total_bytes / 1024u);
    console_printf("heap range:   %x - %x\n", stats.heap_start, stats.heap_end);
    console_printf("heap used:    %u bytes\n", stats.heap_used);
    console_printf("heap free:    %u bytes\n", stats.heap_free);
    console_printf("initrd size:  %u bytes\n", initrd_module_size());
}

static void shell_ticks(void) {
    console_printf("ticks: %u at %u Hz\n", pit_ticks(), pit_frequency());
}

static void shell_hexdump(const char *path) {
    const initrd_file_t *file = initrd_find(path);
    if (!file) {
        console_printf("hexdump: %s: not found\n", path);
        return;
    }
    console_hexdump(file->data, file->size, 0);
}

static void shell_run_binary(int argc, char **argv) {
    int exit_code = task_run_argv(argv[0], argc, (const char *const *)argv);
    if (exit_code == -1) {
        console_printf("run: %s failed to load or execute\n", argv[0]);
        return;
    }
    console_printf("[task] exited with code %d\n", exit_code);
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

    exit_code = task_run_argv(binary_path, task_argc, task_argv);
    if (exit_code < 0) {
        console_printf("%s failed to load or execute\n", argv[0]);
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

    exit_code = task_run_argv(resolved, argc, task_argv);
    if (exit_code < 0) {
        return false;
    }

    console_printf("[task] exited with code %d\n", exit_code);
    return true;
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
        console_printf("%s: unknown command\n", argv[0]);
    } else {
    }
}

void shell_run(void) {
    char line[SHELL_LINE_MAX];

    console_write("Type 'help' if you want a map.\n");
    for (;;) {
        console_write("openhobby> ");
        keyboard_readline(line, sizeof(line));
        shell_execute(line);
    }
}
