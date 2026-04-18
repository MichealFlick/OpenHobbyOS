#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <reent.h>

_fpos64_t __sseek64(struct _reent *reent, void *cookie, _fpos64_t offset, int whence) {
    FILE *fp = (FILE *) cookie;
    off_t result;

    if (offset < (int32_t) INT32_MIN || offset > (int32_t) INT32_MAX) {
        reent->_errno = EOVERFLOW;
        fp->_flags &= ~__SOFF;
        return (_fpos64_t) -1;
    }

    result = lseek(fp->_file, (off_t) offset, whence);
    if (result == (off_t) -1) {
        reent->_errno = errno;
        fp->_flags &= ~__SOFF;
        return (_fpos64_t) -1;
    }

    fp->_flags |= __SOFF;
    fp->_offset = result;
    return (_fpos64_t) result;
}

_READ_WRITE_RETURN_TYPE __swrite64(struct _reent *reent, void *cookie, const char *buffer, _READ_WRITE_BUFSIZE_TYPE length) {
    FILE *fp = (FILE *) cookie;
    ssize_t written;

    if (fp->_flags & __SAPP) {
        if (lseek(fp->_file, 0, SEEK_END) == (off_t) -1) {
            reent->_errno = errno;
            return -1;
        }
    }

    fp->_flags &= ~__SOFF;
    written = write(fp->_file, buffer, length);
    if (written < 0) {
        reent->_errno = errno;
        return -1;
    }

    return (_READ_WRITE_RETURN_TYPE) written;
}
