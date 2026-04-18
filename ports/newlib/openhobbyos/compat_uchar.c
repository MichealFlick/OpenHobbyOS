#include <errno.h>
#include <string.h>
#include <uchar.h>

static mbstate_t oh_uchar_state;

static mbstate_t *oh_uchar_get_state(mbstate_t *ps) {
    if (ps != NULL) {
        return ps;
    }
    return &oh_uchar_state;
}

static void oh_uchar_reset_state(mbstate_t *ps) {
    memset(ps, 0, sizeof(*ps));
}

static int oh_uchar_is_high_surrogate(char16_t value) {
    return value >= 0xD800u && value <= 0xDBFFu;
}

static int oh_uchar_is_low_surrogate(char16_t value) {
    return value >= 0xDC00u && value <= 0xDFFFu;
}

static char32_t oh_uchar_combine_surrogates(char16_t high, char16_t low) {
    return (((char32_t) (high - 0xD800u)) << 10)
           + (char32_t) (low - 0xDC00u)
           + 0x10000u;
}

size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict ps) {
    mbstate_t *state = oh_uchar_get_state(ps);

    if (s == NULL) {
        oh_uchar_reset_state(state);
        return 1;
    }

    oh_uchar_reset_state(state);

    if (c32 <= 0x7Fu) {
        s[0] = (char) c32;
        return 1;
    }
    if (c32 <= 0x7FFu) {
        s[0] = (char) (0xC0u | (unsigned char) (c32 >> 6));
        s[1] = (char) (0x80u | (unsigned char) (c32 & 0x3Fu));
        return 2;
    }
    if (c32 >= 0xD800u && c32 <= 0xDFFFu) {
        errno = EILSEQ;
        return (size_t) -1;
    }
    if (c32 <= 0xFFFFu) {
        s[0] = (char) (0xE0u | (unsigned char) (c32 >> 12));
        s[1] = (char) (0x80u | (unsigned char) ((c32 >> 6) & 0x3Fu));
        s[2] = (char) (0x80u | (unsigned char) (c32 & 0x3Fu));
        return 3;
    }
    if (c32 <= 0x10FFFFu) {
        s[0] = (char) (0xF0u | (unsigned char) (c32 >> 18));
        s[1] = (char) (0x80u | (unsigned char) ((c32 >> 12) & 0x3Fu));
        s[2] = (char) (0x80u | (unsigned char) ((c32 >> 6) & 0x3Fu));
        s[3] = (char) (0x80u | (unsigned char) (c32 & 0x3Fu));
        return 4;
    }

    errno = EILSEQ;
    return (size_t) -1;
}

size_t c16rtomb(char *restrict s, char16_t c16, mbstate_t *restrict ps) {
    mbstate_t *state = oh_uchar_get_state(ps);

    if (s == NULL) {
        oh_uchar_reset_state(state);
        return 1;
    }

    if (state->__count != 0) {
        char16_t high = (char16_t) state->__value.__wch;

        oh_uchar_reset_state(state);
        if (!oh_uchar_is_low_surrogate(c16)) {
            errno = EILSEQ;
            return (size_t) -1;
        }

        return c32rtomb(s, oh_uchar_combine_surrogates(high, c16), state);
    }

    if (oh_uchar_is_high_surrogate(c16)) {
        state->__count = 1;
        state->__value.__wch = (wint_t) c16;
        return 0;
    }
    if (oh_uchar_is_low_surrogate(c16)) {
        errno = EILSEQ;
        return (size_t) -1;
    }

    return c32rtomb(s, (char32_t) c16, state);
}
