#ifndef OHOS_THREAD_H
#define OHOS_THREAD_H

#include "task.h"
#include "idt.h"

/* Thread states */
typedef enum {
    THREAD_STATE_FREE = 0,      /* Unused slot */
    THREAD_STATE_RUNNING,       /* Currently executing */
    THREAD_STATE_READY,         /* Ready to run */
    THREAD_STATE_BLOCKED,       /* Waiting for something */
    THREAD_STATE_ZOMBIE,        /* Exited, waiting to be reaped */
} thread_state_t;

/* Thread structure - shared address space with process */
typedef struct thread {
    /* Identification */
    u32 tid;                    /* Thread ID */
    u32 pid;                    /* Parent process ID */
    
    /* Execution state */
    registers_t regs;           /* Saved CPU state */
    thread_state_t state;
    
    /* Stack info */
    u32 stack_base;             /* Stack bottom */
    u32 stack_size;             /* Stack size */
    
    /* Scheduling */
    u32 priority;               /* Thread priority (0-99, lower = higher priority) */
    u32 time_slice;             /* Remaining time slice */
    
    /* Blocking */
    u32 wait_for_tid;           /* TID we're waiting for (join) */
    int exit_value;             /* Exit code (for join) */
    
    /* Cleanup */
    struct thread *next_free;   /* Free list linkage */
} thread_t;

/* Thread attributes for creation */
typedef struct {
    u32 stack_size;             /* Requested stack size (0 = default) */
    u32 guard_size;             /* Guard page size */
    u32 priority;               /* Thread priority */
} thread_attr_t;

/* Thread APIs */
bool thread_init(void);

/* Thread creation/management */
int thread_create(u32 *tid_out, const thread_attr_t *attr, u32 (*start_func)(void*), void *arg);
void thread_exit(int exit_code) NORETURN;
int thread_join(u32 tid, int *exit_code_out);
int thread_detach(u32 tid);
void thread_yield(void);

/* Thread-local storage */
u32 thread_self(void);  /* Returns current TID */
thread_t *thread_current(void);

/* Process-level thread operations */
void thread_exit_all(void);  /* Called on process exit */

/* Scheduler integration */
thread_t *thread_next_to_run(void);
void thread_schedule(void);

/* Constants */
#define THREAD_MAX_THREADS      64      /* Max threads per process */
#define THREAD_DEFAULT_STACK    (64 * 1024)   /* 64KB default stack */
#define THREAD_MAX_PRIORITY     0       /* Highest (real-time) */
#define THREAD_MIN_PRIORITY     99      /* Lowest (idle) */
#define THREAD_DEFAULT_PRIORITY 50      /* Normal priority */

#endif
