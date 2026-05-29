#include "runtime.h"
#include "syscall.h"

#define NET_GET_MAC 0x4E01

static void print_mac(const unsigned char *mac) {
    char buf[18];
    unsigned int pos = 0;
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        if (i > 0) buf[pos++] = ':';
        buf[pos++] = hex[(mac[i] >> 4) & 0xF];
        buf[pos++] = hex[mac[i] & 0xF];
    }
    buf[pos] = '\n';
    sys_write(1, buf, pos);
}

int main(int argc, char **argv) {
    u_puts("net_test: opening /dev/net...\n");

    int fd = sys_open("/dev/net", 2, 0);
    if (fd < 0) {
        u_puts("net_test: FAIL - open /dev/net: ");
        u_put_int(fd);
        u_puts("\n");
        return 1;
    }
    u_puts("net_test: /dev/net opened\n");

    unsigned char mac[6];
    if (sys_ioctl(fd, NET_GET_MAC, mac) < 0) {
        u_puts("net_test: ioctl GET_MAC failed\n");
    } else {
        u_puts("net_test: MAC = ");
        print_mac(mac);
    }

    unsigned char test_pkt[42];
    u_memset(test_pkt, 0xFF, 6);
    u_memcpy(test_pkt + 6, mac, 6);
    test_pkt[12] = 0x08;
    test_pkt[13] = 0x06;
    test_pkt[14] = 0x00; test_pkt[15] = 0x01;
    test_pkt[16] = 0x08; test_pkt[17] = 0x00;
    test_pkt[18] = 0x06; test_pkt[19] = 0x04;
    test_pkt[20] = 0x00; test_pkt[21] = 0x01;
    u_memcpy(test_pkt + 22, mac, 6);
    test_pkt[28] = 0x00; test_pkt[29] = 0x00; test_pkt[30] = 0x00; test_pkt[31] = 0x00;
    test_pkt[32] = 0x0A; test_pkt[33] = 0x00; test_pkt[34] = 0x02; test_pkt[35] = 0x02;
    u_memset(test_pkt + 36, 0x00, 6);
    test_pkt[38] = 0x0A; test_pkt[39] = 0x00; test_pkt[40] = 0x02; test_pkt[41] = 0x02;

    u_puts("net_test: sending ARP who-has 10.0.2.2...\n");
    int n = sys_write(fd, test_pkt, 42);
    if (n < 0) {
        u_puts("net_test: write failed: ");
        u_put_int(n);
        u_puts("\n");
    } else {
        u_puts("net_test: wrote bytes: ");
        u_put_uint((unsigned int)n);
        u_puts("\n");
    }

    u_puts("net_test: trying to read...\n");
    unsigned char buf[1600];
    for (int i = 0; i < 500; i++) {
        n = sys_read(fd, buf, sizeof(buf));
        if (n > 0) {
            u_puts("net_test: GOT packet! bytes=");
            u_put_uint((unsigned int)n);
            u_puts("\n");
            return 0;
        }
        for (volatile int j = 0; j < 100000; j++);
    }
    u_puts("net_test: no response after 500 retries\n");

    sys_close(fd);
    u_puts("net_test: done\n");
    return 0;
}
