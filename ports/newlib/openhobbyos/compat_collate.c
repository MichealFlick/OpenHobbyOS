#include <limits.h>
#include <string.h>

/*
 * I do not ship external LC_COLLATE tables yet, so callers need the
 * traditional byte-range fallback instead of pretending locale data exists.
 */
int __collate_load_error = 1;

int __collate_range_cmp(int c1, int c2) {
    char left[2];
    char right[2];
    int result;

    c1 &= UCHAR_MAX;
    c2 &= UCHAR_MAX;
    if (c1 == c2) {
        return 0;
    }

    left[0] = (char)c1;
    left[1] = '\0';
    right[0] = (char)c2;
    right[1] = '\0';

    result = strcoll(left, right);
    if (result != 0) {
        return result;
    }

    return c1 - c2;
}
