#ifndef OHOS_SIGNAL_H
#define OHOS_SIGNAL_H

#include "types.h"

/* Linux signal numbers (subset) */
#define SIGHUP     1   /* Hangup */
#define SIGINT     2   /* Interrupt (Ctrl+C) */
#define SIGQUIT    3   /* Quit */
#define SIGILL     4   /* Illegal instruction */
#define SIGTRAP    5   /* Trace trap */
#define SIGABRT    6   /* Abort */
#define SIGFPE     8   /* Floating point exception */
#define SIGKILL    9   /* Kill (cannot be caught) */
#define SIGUSR1   10   /* User signal 1 */
#define SIGSEGV   11   /* Segmentation fault */
#define SIGUSR2   12   /* User signal 2 */
#define SIGPIPE   13   /* Broken pipe */
#define SIGALRM   14   /* Alarm clock */
#define SIGTERM   15   /* Termination */
#define SIGCHLD   17   /* Child status changed */
#define SIGCONT   18   /* Continue stopped process */
#define SIGSTOP   19   /* Stop (cannot be caught) */
#define SIGTSTP   20   /* TTY stop (Ctrl+Z) */
#define SIGTTIN   21   /* TTY read from bg */
#define SIGTTOU   22   /* TTY write to bg */

#define NSIG      32   /* Number of signals */

/* Signal actions */
#define SIG_DFL   ((void (*)(int))0)   /* Default action */
#define SIG_IGN   ((void (*)(int))1)   /* Ignore */
#define SIG_ERR   ((void (*)(int))-1)  /* Error */

/* Signal disposition */
typedef enum {
    SIGACT_DEFAULT = 0,     /* Default action */
    SIGACT_IGNORE,          /* Ignore the signal */
    SIGACT_HANDLER,         /* Call user handler */
    SIGACT_CORE,            /* Core dump */
    SIGACT_TERM,            /* Terminate */
    SIGACT_STOP,            /* Stop process */
    SIGACT_CONT,            /* Continue if stopped */
} signal_action_t;

/* Signal set for sigprocmask */
typedef u32 sigset_t;

/* Signal information structure */
typedef struct {
    int si_signo;           /* Signal number */
    int si_code;            /* Signal code */
    int si_errno;           /* Errno value */
    u32 si_pid;             /* Sending process */
    u32 si_uid;             /* Real user ID of sender */
    void *si_addr;          /* Faulting address (for SIGSEGV) */
} siginfo_t;

/* Per-process signal state */
typedef struct {
    /* Signal handlers (function pointers stored as u32 addresses) */
    u32 handlers[NSIG];
    
    /* Signal mask (blocked signals) */
    sigset_t blocked;
    
    /* Pending signals */
    sigset_t pending;
    
    /* Process is currently handling a signal */
    bool in_signal_handler;
    
    /* Saved signal mask for nested handlers */
    sigset_t saved_mask;
} signal_state_t;

/* Signal APIs */
void signal_init(signal_state_t *state);
bool signal_pending(const signal_state_t *state);
int signal_get_next(signal_state_t *state);
void signal_deliver(signal_state_t *state, int signum, const registers_t *regs);
void signal_return_from_handler(void);

/* Default actions */
void signal_default_term(int signum);   /* Terminate */
void signal_default_core(int signum);   /* Core dump */
void signal_default_stop(int signum);   /* Stop */
void signal_default_cont(int signum);   /* Continue */

#endif
