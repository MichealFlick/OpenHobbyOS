#include "runtime.h"
#include "syscall.h"

#define XNX_FBIOGET_VSCREENINFO 0x4600
#define MAP_SHARED 1
#define PROT_READ 1
#define PROT_WRITE 2
#define O_RDWR 2

struct xnx_fb_var_screeninfo {
    unsigned int xres, yres;
    unsigned int xres_virtual, yres_virtual;
    unsigned int xoffset, yoffset;
    unsigned int bits_per_pixel;
    unsigned int grayscale;
    struct fb_bitfield red, green, blue, transp;
    unsigned int pitch;
};

int main(void) {
    int fb_fd = sys_open("/dev/fb0", O_RDWR, 0);
    if (fb_fd < 0) {
        u_puts("test_fb: FAIL open /dev/fb0\n");
        return 1;
    }

    struct xnx_fb_var_screeninfo vinfo;
    int ret = sys_ioctl(fb_fd, XNX_FBIOGET_VSCREENINFO, &vinfo);
    if (ret < 0) {
        u_puts("test_fb: FAIL ioctl\n");
        sys_close(fb_fd);
        return 1;
    }

    unsigned int bytes_pp = (vinfo.bits_per_pixel + 7u) / 8u;
    unsigned int fb_size = vinfo.pitch > 0 ? vinfo.pitch * vinfo.yres
                                          : vinfo.xres * vinfo.yres * bytes_pp;
    unsigned int *fb = (unsigned int *)sys_mmap2(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((int)fb == -1) {
        u_puts("test_fb: FAIL mmap\n");
        sys_close(fb_fd);
        return 1;
    }

    u_puts("test_fb: mmap OK\n");

    u_memset(fb, 0x00, fb_size);
    u_puts("test_fb: memset OK\n");

    sys_munmap(fb, fb_size);
    sys_close(fb_fd);
    u_puts("test_fb: PASS\n");
    return 0;
}
