#ifndef OPENHOBBYOS_LINUX_ETHTOOL_H
#define OPENHOBBYOS_LINUX_ETHTOOL_H

#include <stdint.h>

#define ETHTOOL_GSET 0x00000001u

struct ethtool_cmd {
    uint32_t cmd;
    uint32_t supported;
    uint32_t advertising;
    uint16_t speed;
    uint8_t duplex;
    uint8_t port;
    uint8_t phy_address;
    uint8_t transceiver;
    uint8_t autoneg;
    uint8_t mdio_support;
    uint32_t maxtxpkt;
    uint32_t maxrxpkt;
    uint16_t speed_hi;
    uint8_t eth_tp_mdix;
    uint8_t reserved2;
    uint32_t lp_advertising;
    uint32_t reserved[2];
};

#endif
