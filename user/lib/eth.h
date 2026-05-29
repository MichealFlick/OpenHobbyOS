#ifndef OHOS_ETH_H
#define OHOS_ETH_H

#include <stdint.h>
#include <sys/types.h>

#define ETH_ALEN 6
#define ETH_MTU 1514
#define ETH_HEADER_LEN 14

#define ETH_P_IP  0x0800
#define ETH_P_ARP 0x0806

typedef struct {
    uint8_t  dest[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

int eth_open(void);
ssize_t eth_send(int fd, const void *buf, size_t len);
ssize_t eth_recv(int fd, void *buf, size_t len);

#endif
