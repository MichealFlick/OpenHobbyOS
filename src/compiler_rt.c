typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef int int32_t;

struct udivmod {
    uint64_t quot;
    uint64_t rem;
};

static struct udivmod udivmoddi4(uint64_t a, uint64_t b)
{
    struct udivmod r = {0, 0};
    if (b == 0) return r;
    if (b == 1) { r.quot = a; return r; }
    uint64_t n = a;
    uint64_t d = b;
    uint64_t q = 0;
    int shift;
    for (shift = 0; shift < 64; shift++)
        if ((d << shift) & 0x8000000000000000ULL) break;
    if (shift == 64) shift = 63;
    for (;;) {
        uint64_t t = d << shift;
        if (n >= t) {
            n -= t;
            q |= 1ULL << shift;
        }
        if (shift == 0) break;
        shift--;
    }
    r.quot = q;
    r.rem = n;
    return r;
}

uint64_t __udivdi3(uint64_t a, uint64_t b)
{
    return udivmoddi4(a, b).quot;
}

uint64_t __umoddi3(uint64_t a, uint64_t b)
{
    return udivmoddi4(a, b).rem;
}

uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *rem)
{
    struct udivmod r = udivmoddi4(a, b);
    if (rem) *rem = r.rem;
    return r.quot;
}

int32_t __ffsdi2(uint64_t x)
{
    if (x == 0) return 0;
    int32_t n = 1;
    if ((x & 0xFFFFFFFFULL) == 0) { n += 32; x >>= 32; }
    if ((x & 0xFFFFULL) == 0) { n += 16; x >>= 16; }
    if ((x & 0xFFULL) == 0) { n += 8; x >>= 8; }
    if ((x & 0xFULL) == 0) { n += 4; x >>= 4; }
    if ((x & 0x3ULL) == 0) { n += 2; x >>= 2; }
    if ((x & 0x1ULL) == 0) { n += 1; }
    return n;
}
