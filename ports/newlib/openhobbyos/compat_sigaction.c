#include <errno.h>
#include <signal.h>
#include <string.h>

#ifndef NSIG
#define NSIG 64
#endif

static struct sigaction oh_signal_actions[NSIG];

int sigaction(int signo, const struct sigaction *act, struct sigaction *oldact) {
    if (signo <= 0 || signo >= NSIG || signo == SIGKILL || signo == SIGSTOP) {
        errno = EINVAL;
        return -1;
    }

    if (oldact != NULL) {
        memcpy(oldact, &oh_signal_actions[signo], sizeof(*oldact));
    }

    if (act != NULL) {
        memcpy(&oh_signal_actions[signo], act, sizeof(*act));
    }

    return 0;
}
