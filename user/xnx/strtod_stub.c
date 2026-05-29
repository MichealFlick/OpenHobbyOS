struct _reent;

double _strtod_r(struct _reent *r, const char *s, char **out) {
    if (out) *out = (char *)s;
    (void)r;
    return 0.0;
}
