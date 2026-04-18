#include "runtime.h"
#include "syscall.h"

int main(int argc, char **argv, char **envp) {
    struct linux_utsname name;

    (void)argc;
    (void)argv;
    (void)envp;

    if (sys_uname(&name) != 0) {
        u_puts("uname syscall failed\n");
        return 1;
    }

    u_print_uname(&name);
    return 0;
}
