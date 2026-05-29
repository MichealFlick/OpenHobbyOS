#ifndef OHOS_ASSERT_H
#define OHOS_ASSERT_H

#ifndef NDEBUG
#define assert(x) do { if (!(x)) { for (;;) { __asm__ volatile ("hlt"); } } } while (0)
#else
#define assert(x) ((void)0)
#endif

#endif
