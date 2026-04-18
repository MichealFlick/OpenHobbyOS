#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

static char oh_dlerror_buffer[128];
static int oh_dlerror_ready;

static void oh_set_dlerror(const char *message) {
    strncpy(oh_dlerror_buffer, message, sizeof(oh_dlerror_buffer) - 1);
    oh_dlerror_buffer[sizeof(oh_dlerror_buffer) - 1] = '\0';
    oh_dlerror_ready = 1;
}

void *dlopen(const char *path, int mode) {
    (void) path;
    (void) mode;

    errno = ENOSYS;
    oh_set_dlerror("dynamic loading is not supported on OpenHobbyOS yet");
    return NULL;
}

int dlclose(void *handle) {
    if (handle == NULL) {
        return 0;
    }

    errno = ENOSYS;
    oh_set_dlerror("there is no shared-object loader to close handles for");
    return -1;
}

void *dlsym(void *handle, const char *symbol) {
    (void) handle;
    (void) symbol;

    errno = ENOSYS;
    oh_set_dlerror("dlsym needs shared-object support, and we are static-only");
    return NULL;
}

char *dlerror(void) {
    if (!oh_dlerror_ready) {
        return NULL;
    }

    oh_dlerror_ready = 0;
    return oh_dlerror_buffer;
}

void *dlvsym(void *handle, const char *symbol, const char *version) {
    (void) version;
    return dlsym(handle, symbol);
}

int dladdr(const void *addr, Dl_info *info) {
    (void) addr;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }

    return 0;
}

int dladdr1(const void *addr, Dl_info *info, void **extra_info, int flags) {
    (void) extra_info;
    (void) flags;
    return dladdr(addr, info);
}
