#ifndef OPENHOBBYOS_DIRENT_H
#define OPENHOBBYOS_DIRENT_H

#include <sys/cdefs.h>
#include <sys/dirent.h>

__BEGIN_DECLS
int alphasort(const struct dirent **left, const struct dirent **right);
int closedir(DIR *dir);
int dirfd(DIR *dir);
int fdclosedir(DIR *dir);
DIR *fdopendir(int fd);
DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
struct dirent64 *readdir64(DIR *dir);
int readdir_r(DIR *dir, struct dirent *entry, struct dirent **result);
void rewinddir(DIR *dir);
int scandir(const char *path, struct dirent ***entries, int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **));
int scandirat(int dirfd, const char *path, struct dirent ***entries, int (*filter)(const struct dirent *),
              int (*compar)(const struct dirent **, const struct dirent **));
void seekdir(DIR *dir, long offset);
long telldir(DIR *dir);
int versionsort(const struct dirent **left, const struct dirent **right);
__END_DECLS

#endif
