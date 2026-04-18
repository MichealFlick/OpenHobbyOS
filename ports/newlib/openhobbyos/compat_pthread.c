#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define OH_PTHREAD_MAX_THREADS 16
#define OH_PTHREAD_MAIN_ID     ((pthread_t) 1)
#define OH_PTHREAD_NAME_MAX    16

typedef struct {
    int used;
    int detached;
    int completed;
    pthread_t id;
    void *result;
    char name[OH_PTHREAD_NAME_MAX];
    jmp_buf exit_jmp;
} oh_pthread_slot_t;

static oh_pthread_slot_t oh_pthread_slots[OH_PTHREAD_MAX_THREADS];
static pthread_t oh_pthread_next_id = OH_PTHREAD_MAIN_ID + 1;
static pthread_t oh_pthread_current_id = OH_PTHREAD_MAIN_ID;
static int oh_pthread_current_slot = -1;
static char oh_pthread_main_name[OH_PTHREAD_NAME_MAX] = "main";

static void oh_pthread_copy_name(char *dest, const char *src) {
    if (dest == NULL) {
        return;
    }

    if (src == NULL || *src == '\0') {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, OH_PTHREAD_NAME_MAX - 1);
    dest[OH_PTHREAD_NAME_MAX - 1] = '\0';
}

static oh_pthread_slot_t *oh_pthread_find_slot(pthread_t id) {
    for (int i = 0; i < OH_PTHREAD_MAX_THREADS; ++i) {
        if (oh_pthread_slots[i].used && oh_pthread_slots[i].id == id) {
            return &oh_pthread_slots[i];
        }
    }

    return NULL;
}

static oh_pthread_slot_t *oh_pthread_allocate_slot(void) {
    for (int i = 0; i < OH_PTHREAD_MAX_THREADS; ++i) {
        if (!oh_pthread_slots[i].used) {
            memset(&oh_pthread_slots[i], 0, sizeof(oh_pthread_slots[i]));
            oh_pthread_slots[i].used = 1;
            oh_pthread_slots[i].id = oh_pthread_next_id++;
            if (oh_pthread_next_id == 0) {
                oh_pthread_next_id = OH_PTHREAD_MAIN_ID + 1;
            }
            oh_pthread_copy_name(oh_pthread_slots[i].name, "worker");
            return &oh_pthread_slots[i];
        }
    }

    return NULL;
}

static void oh_pthread_release_slot(oh_pthread_slot_t *slot) {
    if (slot == NULL) {
        return;
    }

    memset(slot, 0, sizeof(*slot));
}

static int oh_pthread_prepare_mutex(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        return EINVAL;
    }

    if (*mutex == _PTHREAD_MUTEX_INITIALIZER) {
        *mutex = 0;
    }

    return 0;
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
    (void) prepare;
    (void) parent;
    (void) child;
    return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
    return sigprocmask(how, set, oldset);
}

pthread_t pthread_self(void) {
    return oh_pthread_current_id;
}

int pthread_equal(pthread_t left, pthread_t right) {
    return left == right;
}

int pthread_getname_np(pthread_t thread, char *name, size_t size) {
    const char *src;
    oh_pthread_slot_t *slot;
    size_t length;

    if (name == NULL || size == 0) {
        return ERANGE;
    }

    if (thread == OH_PTHREAD_MAIN_ID) {
        src = oh_pthread_main_name;
    } else {
        slot = oh_pthread_find_slot(thread);
        if (slot == NULL) {
            return ESRCH;
        }
        src = slot->name;
    }

    length = strlen(src);
    if (length + 1 > size) {
        return ERANGE;
    }

    memcpy(name, src, length + 1);
    return 0;
}

int pthread_setname_np(pthread_t thread, const char *name) {
    oh_pthread_slot_t *slot;

    if (name == NULL) {
        return EINVAL;
    }

    if (thread == OH_PTHREAD_MAIN_ID) {
        oh_pthread_copy_name(oh_pthread_main_name, name);
        return 0;
    }

    slot = oh_pthread_find_slot(thread);
    if (slot == NULL) {
        return ESRCH;
    }

    oh_pthread_copy_name(slot->name, name);
    return 0;
}

int pthread_attr_init(pthread_attr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }

    memset(attr, 0, sizeof(*attr));
    attr->is_initialized = 1;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->contentionscope = PTHREAD_SCOPE_SYSTEM;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }

    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_attr_setscope(pthread_attr_t *attr, int scope) {
    if (attr == NULL) {
        return EINVAL;
    }

    attr->contentionscope = scope;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    if (attr == NULL) {
        return EINVAL;
    }

    attr->detachstate = detachstate;
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }

    memset(attr, 0, sizeof(*attr));
    attr->is_initialized = 1;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }

    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind) {
    if (attr == NULL) {
        return EINVAL;
    }

#if defined(_UNIX98_THREAD_MUTEX_ATTRIBUTES)
    attr->type = kind;
    attr->recursive = (kind == PTHREAD_MUTEX_RECURSIVE);
#else
    (void) kind;
    attr->recursive = 0;
#endif
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void) attr;
    if (mutex == NULL) {
        return EINVAL;
    }

    *mutex = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        return EINVAL;
    }

    *mutex = _PTHREAD_MUTEX_INITIALIZER;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    int status = oh_pthread_prepare_mutex(mutex);
    if (status != 0) {
        return status;
    }

    if (*mutex == UINT32_MAX) {
        return EAGAIN;
    }

    ++(*mutex);
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    int status = oh_pthread_prepare_mutex(mutex);
    if (status != 0) {
        return status;
    }

    if (*mutex == 0) {
        return EPERM;
    }

    --(*mutex);
    return 0;
}

int pthread_condattr_init(pthread_condattr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }

    memset(attr, 0, sizeof(*attr));
    attr->is_initialized = 1;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }

    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void) attr;
    if (cond == NULL) {
        return EINVAL;
    }

    *cond = 0;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    if (cond == NULL) {
        return EINVAL;
    }

    *cond = _PTHREAD_COND_INITIALIZER;
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    if (cond == NULL) {
        return EINVAL;
    }

    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    if (cond == NULL) {
        return EINVAL;
    }

    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (cond == NULL || mutex == NULL) {
        return EINVAL;
    }

    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    (void) abstime;
    return pthread_cond_wait(cond, mutex);
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (once_control == NULL || init_routine == NULL) {
        return EINVAL;
    }

    if (!once_control->init_executed) {
        once_control->init_executed = 1;
        init_routine();
    }

    return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    oh_pthread_slot_t *slot;
    pthread_t previous_id;
    int previous_slot;
    void *result = NULL;

    if (thread == NULL || start_routine == NULL) {
        return EINVAL;
    }

    slot = oh_pthread_allocate_slot();
    if (slot == NULL) {
        return EAGAIN;
    }

    slot->detached = (attr != NULL && attr->detachstate == PTHREAD_CREATE_DETACHED);
    *thread = slot->id;

    previous_id = oh_pthread_current_id;
    previous_slot = oh_pthread_current_slot;
    oh_pthread_current_id = slot->id;
    oh_pthread_current_slot = (int) (slot - oh_pthread_slots);

    if (setjmp(slot->exit_jmp) == 0) {
        result = start_routine(arg);
    } else {
        result = slot->result;
    }

    slot->result = result;
    slot->completed = 1;

    oh_pthread_current_id = previous_id;
    oh_pthread_current_slot = previous_slot;

    if (slot->detached) {
        oh_pthread_release_slot(slot);
    }

    return 0;
}

int pthread_join(pthread_t thread, void **value_ptr) {
    oh_pthread_slot_t *slot = oh_pthread_find_slot(thread);

    if (slot == NULL) {
        return ESRCH;
    }

    if (value_ptr != NULL) {
        *value_ptr = slot->result;
    }

    oh_pthread_release_slot(slot);
    return 0;
}

int pthread_detach(pthread_t thread) {
    oh_pthread_slot_t *slot = oh_pthread_find_slot(thread);

    if (slot == NULL) {
        return ESRCH;
    }

    slot->detached = 1;
    if (slot->completed) {
        oh_pthread_release_slot(slot);
    }

    return 0;
}

void pthread_exit(void *value_ptr) {
    if (oh_pthread_current_slot >= 0 && oh_pthread_current_slot < OH_PTHREAD_MAX_THREADS) {
        oh_pthread_slots[oh_pthread_current_slot].result = value_ptr;
        longjmp(oh_pthread_slots[oh_pthread_current_slot].exit_jmp, 1);
    }

    _exit(0);
}
