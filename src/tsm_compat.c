#include "memory.h"
#include "string.h"

int errno = 0;

void *malloc(size_t size) {
    return kmalloc(size);
}

void free(void *ptr) {
    kfree(ptr);
}

void *calloc(size_t count, size_t size) {
    return kcalloc(count, size);
}

void *realloc(void *ptr, size_t new_size) {
    return krealloc(ptr, new_size);
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *copy = kmalloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}
