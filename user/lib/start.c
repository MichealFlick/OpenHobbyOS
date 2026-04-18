#include "syscall.h"

extern int main(int argc, char **argv, char **envp);

void _start(void) {
    int argc;
    char **argv;
    char **envp;

    __asm__ volatile ("movl (%%esp), %0" : "=r"(argc));
    __asm__ volatile ("leal 4(%%esp), %0" : "=r"(argv));
    envp = argv + argc + 1;

    sys_exit_group(main(argc, argv, envp));
    for (;;) {
    }
}
