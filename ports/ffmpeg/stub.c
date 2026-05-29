/* Minimal startup stub for FFmpeg configure link test.
 * Provides _start and other symbols needed to link a trivial
 * 'int main(void) { return 0; }' program with the OHOS toolchain. */

void _start(void) {
    __asm__ volatile (
        "call main\n"
        "mov %0, %%ebx\n"
        "mov $1, %%eax\n"
        "int $0x80\n"
        :: "i"(0) : "eax", "ebx"
    );
}

void __swrite64(void) {}
void __sseek64(void) {}
