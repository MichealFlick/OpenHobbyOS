#ifndef OPENHOBBYOS_MNTENT_H
#define OPENHOBBYOS_MNTENT_H

#include <stdio.h>

struct mntent {
    char *mnt_fsname;
    char *mnt_dir;
    char *mnt_type;
    char *mnt_opts;
    int mnt_freq;
    int mnt_passno;
};

#define MNTOPT_RO "ro"
#define MNTOPT_RW "rw"

FILE *setmntent(const char *path, const char *mode);
int endmntent(FILE *fp);
struct mntent *getmntent(FILE *fp);
struct mntent *getmntent_r(FILE *fp, struct mntent *result, char *buffer, int buflen);
char *hasmntopt(const struct mntent *mnt, const char *opt);

#endif
