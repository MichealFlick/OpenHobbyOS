#ifndef OHOS_NETDEV_H
#define OHOS_NETDEV_H

#include "types.h"

#define NETDEV_NAME_MAX 16
#define NETDEV_MAX 4
#define NETDEV_MTU 1514
#define NETDEV_RX_QUEUE_SIZE 32
#define NETDEV_RX_BUF_SIZE 1536

#define OHOS_NETDEV_GET_MAC 0x4E01

typedef struct netdev netdev_t;

typedef int (*netdev_send_t)(netdev_t *dev, const u8 *packet, u16 length);
typedef void (*netdev_rx_enable_t)(netdev_t *dev, bool enable);

typedef struct {
    netdev_send_t send;
    netdev_rx_enable_t rx_enable;
} netdev_ops_t;

struct netdev {
    u32 id;
    char name[NETDEV_NAME_MAX];
    u8 mac[6];
    u16 mtu;
    const netdev_ops_t *ops;
    void *private;
    bool present;
    bool link_up;
    u8 rx_data[NETDEV_RX_QUEUE_SIZE][NETDEV_RX_BUF_SIZE];
    u16 rx_len[NETDEV_RX_QUEUE_SIZE];
    u32 rx_head;
    u32 rx_tail;
};

void netdev_init(void);
int netdev_register(const char *name, const u8 *mac, const netdev_ops_t *ops, void *private);
netdev_t *netdev_by_id(u32 id);
netdev_t *netdev_first(void);
int netdev_rx_push(netdev_t *dev, const u8 *data, u16 length);
int netdev_rx_pop(netdev_t *dev, u8 *buffer, u16 *length);
int netdev_send(netdev_t *dev, const u8 *packet, u16 length);

#endif
