/* Debug override: intercept table overflow to print the size */
#ifdef LUA_CORE
#include <stdio.h>
static inline void __attribute__((used)) _ltable_overflow_dbg(unsigned int size) {
    fprintf(stderr, "\n[LUA_OVERFLOW] size=%u lsize=%d MAXHBITS=%d\n",
            size, (size == 0) ? 0 : (__builtin_clz(size-1) ^ 31) + 1,
            MAXHBITS);
}
/* Override luaG_runerror when it comes from this file */
#define luaG_runerror(L, msg) do { \
    extern void luaG_runerror_(lua_State*, const char*); \
    _ltable_overflow_dbg(0xDEAD); \
    luaG_runerror_(L, msg); \
} while(0)
#endif
