#include <ctype.h>
#include <mntent.h>
#include <stdlib.h>
#include <string.h>

static char *oh_next_token(char **cursor) {
    char *start = *cursor;

    while (*start && isspace((unsigned char) *start)) {
        start++;
    }
    if (*start == '\0' || *start == '#') {
        *cursor = start;
        return NULL;
    }

    *cursor = start;
    while (**cursor && !isspace((unsigned char) **cursor)) {
        (*cursor)++;
    }
    if (**cursor) {
        *(*cursor)++ = '\0';
    }
    return start;
}

FILE *setmntent(const char *path, const char *mode) {
    return fopen(path, mode);
}

int endmntent(FILE *fp) {
    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

struct mntent *getmntent_r(FILE *fp, struct mntent *result, char *buffer, int buflen) {
    char *cursor;
    char *value;

    if (fp == NULL || result == NULL || buffer == NULL || buflen <= 0) {
        return NULL;
    }

    while (fgets(buffer, buflen, fp)) {
        cursor = buffer;
        value = oh_next_token(&cursor);
        if (value == NULL) {
            continue;
        }
        result->mnt_fsname = value;

        value = oh_next_token(&cursor);
        if (value == NULL) {
            continue;
        }
        result->mnt_dir = value;

        value = oh_next_token(&cursor);
        if (value == NULL) {
            continue;
        }
        result->mnt_type = value;

        value = oh_next_token(&cursor);
        if (value == NULL) {
            continue;
        }
        result->mnt_opts = value;

        value = oh_next_token(&cursor);
        result->mnt_freq = value ? atoi(value) : 0;

        value = oh_next_token(&cursor);
        result->mnt_passno = value ? atoi(value) : 0;
        return result;
    }

    return NULL;
}

struct mntent *getmntent(FILE *fp) {
    static char buffer[512];
    static struct mntent result;
    return getmntent_r(fp, &result, buffer, (int) sizeof(buffer));
}

char *hasmntopt(const struct mntent *mnt, const char *opt) {
    size_t opt_len;
    const char *cursor;

    if (mnt == NULL || mnt->mnt_opts == NULL || opt == NULL) {
        return NULL;
    }

    opt_len = strlen(opt);
    cursor = mnt->mnt_opts;
    while (*cursor) {
        const char *start = cursor;
        while (*cursor && *cursor != ',') {
            cursor++;
        }
        if ((size_t) (cursor - start) == opt_len && strncmp(start, opt, opt_len) == 0) {
            return (char *) start;
        }
        if (*cursor == ',') {
            cursor++;
        }
    }

    return NULL;
}
