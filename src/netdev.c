#include "netdev.h"
#include "console.h"
#include "string.h"

static netdev_t netdevs[NETDEV_MAX];
static u32 netdev_count;
static bool initialized;

void netdev_init(void) {
    for (u32 i = 0; i < NETDEV_MAX; i++) {
        netdevs[i].present = false;
    }
    netdev_count = 0;
    initialized = true;
}

int netdev_register(const char *name, const u8 *mac, const netdev_ops_t *ops, void *private) {
    if (!initialized || netdev_count >= NETDEV_MAX) return -1;
    netdev_t *dev = &netdevs[netdev_count];
    dev->id = netdev_count;
    u32 i;
    for (i = 0; name[i] && i < NETDEV_NAME_MAX - 1; i++) {
        dev->name[i] = name[i];
    }
    dev->name[i] = '\0';
    for (i = 0; i < 6; i++) dev->mac[i] = mac[i];
    dev->mtu = NETDEV_MTU;
    dev->ops = ops;
    dev->private = private;
    dev->present = true;
    dev->link_up = false;
    dev->rx_head = 0;
    dev->rx_tail = 0;
    netdev_count++;
    console_printf("[netdev] registered %s (mac=%02x:%02x:%02x:%02x:%02x:%02x)\n",
                   name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (int)dev->id;
}

netdev_t *netdev_by_id(u32 id) {
    if (id >= netdev_count || !netdevs[id].present) return NULL;
    return &netdevs[id];
}

netdev_t *netdev_first(void) {
    for (u32 i = 0; i < netdev_count; i++) {
        if (netdevs[i].present) return &netdevs[i];
    }
    return NULL;
}

int netdev_rx_push(netdev_t *dev, const u8 *data, u16 length) {
    if (!dev || !dev->present) return -1;
    if (length > NETDEV_RX_BUF_SIZE) length = NETDEV_RX_BUF_SIZE;
    u32 next = (dev->rx_head + 1) % NETDEV_RX_QUEUE_SIZE;
    if (next == dev->rx_tail) return -1;
    memcpy(dev->rx_data[dev->rx_head], data, length);
    dev->rx_len[dev->rx_head] = length;
    dev->rx_head = next;
    return 0;
}

int netdev_rx_pop(netdev_t *dev, u8 *buffer, u16 *length) {
    if (!dev || !dev->present || dev->rx_tail == dev->rx_head) return -1;
    u16 len = dev->rx_len[dev->rx_tail];
    memcpy(buffer, dev->rx_data[dev->rx_tail], len);
    *length = len;
    dev->rx_tail = (dev->rx_tail + 1) % NETDEV_RX_QUEUE_SIZE;
    return 0;
}

int netdev_send(netdev_t *dev, const u8 *packet, u16 length) {
    if (!dev || !dev->present || !dev->ops || !dev->ops->send) return -1;
    return dev->ops->send(dev, packet, length);
}
