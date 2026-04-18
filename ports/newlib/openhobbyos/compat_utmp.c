#include <stdio.h>
#include <string.h>
#include <sys/utmp.h>

static FILE *oh_utmp_file;
static struct utmp oh_utmp_entry;

static FILE *oh_utmp_open(void) {
    static const char *const paths[] = {
        "/var/run/utmp",
        "/run/utmp",
        NULL,
    };

    if (oh_utmp_file != NULL) {
        return oh_utmp_file;
    }

    for (size_t i = 0; paths[i] != NULL; ++i) {
        oh_utmp_file = fopen(paths[i], "rb");
        if (oh_utmp_file != NULL) {
            return oh_utmp_file;
        }
    }

    return NULL;
}

void setutent(void) {
    FILE *file = oh_utmp_open();

    if (file != NULL) {
        rewind(file);
    }
}

void endutent(void) {
    if (oh_utmp_file != NULL) {
        fclose(oh_utmp_file);
        oh_utmp_file = NULL;
    }
}

struct utmp *getutent(void) {
    FILE *file = oh_utmp_open();

    if (file == NULL) {
        return NULL;
    }

    memset(&oh_utmp_entry, 0, sizeof(oh_utmp_entry));
    if (fread(&oh_utmp_entry, sizeof(oh_utmp_entry), 1, file) != 1) {
        return NULL;
    }

    return &oh_utmp_entry;
}

void setutxent(void) {
    setutent();
}

void endutxent(void) {
    endutent();
}

struct utmp *getutxent(void) {
    return getutent();
}
