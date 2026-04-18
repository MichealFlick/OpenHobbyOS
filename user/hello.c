#include "runtime.h"
#include "syscall.h"

int main(int argc, char **argv, char **envp) {
    char line[96];
    int read_count;

    (void)envp;

    u_puts("hello from /bin/hello\n");
    u_puts("pid: ");
    u_put_uint((unsigned int)sys_getpid());
    u_puts("\nargc: ");
    u_put_uint((unsigned int)argc);
    u_puts("\nargv0: ");
    u_puts((argc > 0 && argv[0]) ? argv[0] : "<none>");
    u_puts("\n");
    u_puts("This binary is an ELF32 user task speaking over int 0x80.\n");
    u_puts("Type a line and I will bounce it back.\n> ");

    read_count = sys_read(0, line, sizeof(line));
    if (read_count > 0) {
        u_puts("echo: ");
        u_putsn(line, (unsigned int)read_count);
        u_puts("\n");
    }

    return 0;
}
