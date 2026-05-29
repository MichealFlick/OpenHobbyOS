#include "runtime.h"
#include "syscall.h"

#define NET_GET_MAC 0x4E01

int main(void) {
    u_puts("Network Information\n");
    u_puts("==================\n\n");

    int fd = sys_open("/dev/net", 2, 0);
    if (fd < 0) {
        u_puts("/dev/net: NOT AVAILABLE\n");
        u_puts("Check boot log for [pci] and [rtl8139] messages.\n");
        return 1;
    }

    unsigned char mac[6];
    if (sys_ioctl(fd, NET_GET_MAC, mac) == 0) {
        u_puts("Interface: eth0\n");
        u_puts("MAC Addr:  ");
        static const char hex[] = "0123456789abcdef";
        for (int i = 0; i < 6; i++) {
            if (i > 0) {
                char col = ':';
                sys_write(1, &col, 1);
            }
            char b[2];
            b[0] = hex[(mac[i] >> 4) & 0xF];
            b[1] = hex[mac[i] & 0xF];
            sys_write(1, b, 2);
        }
        u_puts("\nStatus:    driver loaded, device present\n");
    } else {
        u_puts("Interface: eth0 (MAC unavailable)\n");
    }

    sys_close(fd);
    return 0;
}
