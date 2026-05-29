#include <stddef.h>
#include "syscall.h"

extern int main(int argc, char **argv, char **envp);
extern void (*__init_array_start []) (void);
extern void (*__init_array_end []) (void);
char **environ __attribute__((weak));

#define START_BOOTSTRAP_BYTES 16384u
#define START_BOOTSTRAP_PTRS  64u

static char bootstrap_bytes[START_BOOTSTRAP_BYTES];
static char *bootstrap_argv[START_BOOTSTRAP_PTRS];
static char *bootstrap_envp[START_BOOTSTRAP_PTRS];

static unsigned int start_strlen(const char *text) {
    unsigned int length = 0;

    if (!text) {
        return 0;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

static char *start_copy_string(char **cursor, unsigned int *remaining, const char *text) {
    unsigned int length = start_strlen(text) + 1u;
    char *target;

    if (length == 0 || *remaining < length) {
        return 0;
    }

    target = *cursor;
    for (unsigned int i = 0; i < length; ++i) {
        target[i] = text[i];
    }

    *cursor += length;
    *remaining -= length;
    return target;
}

static int start_copy_vectors(int argc, char **argv, char **envp, char ***argv_out, char ***envp_out) {
    char *cursor = bootstrap_bytes;
    unsigned int remaining = START_BOOTSTRAP_BYTES;
    int envc = 0;

    if (argc < 0 || argc + 1 >= (int)START_BOOTSTRAP_PTRS) {
        return 0;
    }

    while (envp && envp[envc]) {
        envc++;
        if (envc + 1 >= (int)START_BOOTSTRAP_PTRS) {
            return 0;
        }
    }

    for (int i = 0; i < argc; ++i) {
        bootstrap_argv[i] = start_copy_string(&cursor, &remaining, argv[i]);
        if (!bootstrap_argv[i]) {
            return 0;
        }
    }
    bootstrap_argv[argc] = 0;

    for (int i = 0; i < envc; ++i) {
        bootstrap_envp[i] = start_copy_string(&cursor, &remaining, envp[i]);
        if (!bootstrap_envp[i]) {
            return 0;
        }
    }
    bootstrap_envp[envc] = 0;

    *argv_out = bootstrap_argv;
    *envp_out = bootstrap_envp;
    return 1;
}

static void call_init_array(void) {
    size_t count = __init_array_end - __init_array_start;
    for (size_t i = 0; i < count; i++) {
        __init_array_start[i]();
    }
}

__attribute__((noreturn, used))
void start_c(int argc, char **argv, char **envp) {
    char **stable_argv = argv;
    char **stable_envp = envp;

    if (start_copy_vectors(argc, argv, envp, &stable_argv, &stable_envp)) {
        environ = stable_envp;
    } else {
        environ = envp;
    }

    call_init_array();
    sys_exit_group(main(argc, stable_argv, stable_envp));
    for (;;) {
    }
}

__attribute__((noreturn, naked))
void _start(void) {
    __asm__ volatile (
        "movl (%%esp), %%eax\n"
        "leal 4(%%esp), %%edx\n"
        "leal 4(%%edx,%%eax,4), %%ecx\n"
        "pushl %%ecx\n"
        "pushl %%edx\n"
        "pushl %%eax\n"
        "call start_c\n"
        :
        :
        : "eax", "ecx", "edx", "memory"
    );
}
