#include "runtime.h"
#include "syscall.h"

static int hello_run_pulse(int argc, char **argv) {
    char line[96];
    struct linux_timespec req;
    int ok = 0;
    unsigned int rounds;
    unsigned int delay_seconds;
    int pid = sys_getpid();

    if (argc != 4) {
        u_puts("hello: usage: hello pulse <rounds> <delay-seconds>\n");
        return 1;
    }

    rounds = u_parse_uint(argv[2], &ok);
    if (!ok || rounds == 0) {
        u_puts("hello: rounds must be a positive decimal\n");
        return 1;
    }

    delay_seconds = u_parse_uint(argv[3], &ok);
    if (!ok) {
        u_puts("hello: delay must be a decimal number of seconds\n");
        return 1;
    }

    req.tv_sec = (int)delay_seconds;
    req.tv_nsec = 0;

    for (unsigned int index = 0; index < rounds; ++index) {
        unsigned int used = 0;

        used = u_append_text(line, sizeof(line), used, "hello pulse pid=");
        used = u_append_int(line, sizeof(line), used, pid);
        used = u_append_text(line, sizeof(line), used, " step=");
        used = u_append_uint(line, sizeof(line), used, index + 1u);
        used = u_append_text(line, sizeof(line), used, "/");
        used = u_append_uint(line, sizeof(line), used, rounds);
        used = u_append_text(line, sizeof(line), used, "\n");
        u_write_buffer(line, used);

        if (delay_seconds == 0) {
            sys_sched_yield();
        } else if (index + 1u < rounds) {
            sys_nanosleep(&req, 0);
        }
    }

    return 0;
}

int main(int argc, char **argv, char **envp) {
    char line[96];
    int read_count;

    (void)envp;

    if (argc >= 2 && u_strcmp(argv[1], "pulse") == 0) {
        return hello_run_pulse(argc, argv);
    }

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
