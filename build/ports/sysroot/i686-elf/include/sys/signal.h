#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "_ansi.h"
#include <sys/cdefs.h>
#include <sys/features.h>
#include <sys/types.h>
#include <sys/_sigset.h>
#include <sys/_timespec.h>

#if !defined(_SIGSET_T_DECLARED)
#define _SIGSET_T_DECLARED
typedef __sigset_t sigset_t;
#endif

typedef void (*_sig_func_ptr)(int);

union sigval {
    int sival_int;
    void *sival_ptr;
};

/*
 * Linux-facing ports keep tripping over the ultra-tiny signal surface.
 * We expose the common public pieces here even though delivery is still
 * handled by our simpler compat layer underneath.
 */
typedef struct {
    int si_signo;
    int si_code;
    pid_t si_pid;
    uid_t si_uid;
    union sigval si_value;
    void *si_addr;
} siginfo_t;

typedef struct sigaltstack {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;

struct sigaction {
    union {
        _sig_func_ptr _handler;
        void (*_sigaction)(int, siginfo_t *, void *);
    } _signal_handlers;
    sigset_t sa_mask;
    int sa_flags;
};

#define sa_handler _signal_handlers._handler
#define sa_sigaction _signal_handlers._sigaction

#define SA_NOCLDSTOP 0x0001
#define SA_NOCLDWAIT 0x0002
#define SA_SIGINFO   0x0004
#define SA_RESTORER  0x04000000
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000

#define SS_ONSTACK 0x1
#define SS_DISABLE 0x2

#ifndef MINSIGSTKSZ
#define MINSIGSTKSZ 2048
#endif

#ifndef SIGSTKSZ
#define SIGSTKSZ 8192
#endif

#define SIG_SETMASK 0
#define SIG_BLOCK   1
#define SIG_UNBLOCK 2

#define SI_USER    0
#define SI_QUEUE   -1
#define SI_TIMER   -2
#define SI_ASYNCIO -4
#define SI_MESGQ   -3

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int kill(pid_t pid, int sig);
int sigaction(int signo, const struct sigaction *act, struct sigaction *oldact);
int sigaddset(sigset_t *set, int signo);
int sigdelset(sigset_t *set, int signo);
int sigismember(const sigset_t *set, int signo);
int sigfillset(sigset_t *set);
int sigemptyset(sigset_t *set);
int sigpending(sigset_t *set);
int sigsuspend(const sigset_t *set);
int sigwait(const sigset_t *set, int *sig);
int sigaltstack(const stack_t *restrict ss, stack_t *restrict old_ss);
int sigwaitinfo(const sigset_t *set, siginfo_t *info);
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);
int sigqueue(pid_t pid, int signo, const union sigval value);

#define sigaddset(what, sig) (*(what) |= (1UL << (sig)), 0)
#define sigdelset(what, sig) (*(what) &= ~(1UL << (sig)), 0)
#define sigemptyset(what) (*(what) = 0, 0)
#define sigfillset(what) (*(what) = ~(0UL), 0)
#define sigismember(what, sig) (((*(what)) & (1UL << (sig))) != 0)

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
#define SIGIOT   6
#define SIGABRT  6
#define SIGBUS   7
#define SIGFPE   8
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGSTKFLT 16
#define SIGCHLD  17
#define SIGCLD   SIGCHLD
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20
#define SIGTTIN  21
#define SIGTTOU  22
#define SIGURG   23
#define SIGXCPU  24
#define SIGXFSZ  25
#define SIGVTALRM 26
#define SIGPROF  27
#define SIGWINCH 28
#define SIGIO    29
#define SIGPOLL  SIGIO
#define SIGPWR   30
#define SIGSYS   31

#ifndef NSIG
#define NSIG 32
#endif

#ifdef __cplusplus
}
#endif

#endif
