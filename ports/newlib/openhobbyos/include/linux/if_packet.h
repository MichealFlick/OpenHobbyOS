#ifndef OPENHOBBYOS_LINUX_IF_PACKET_H
#define OPENHOBBYOS_LINUX_IF_PACKET_H

#include <sys/socket.h>

#define PACKET_HOST      0
#define PACKET_BROADCAST 1
#define PACKET_MULTICAST 2
#define PACKET_OTHERHOST 3
#define PACKET_OUTGOING  4
#define PACKET_LOOPBACK  5
#define PACKET_FASTROUTE 6

#define PACKET_MR_MULTICAST 0
#define PACKET_MR_PROMISC   1
#define PACKET_MR_ALLMULTI  2
#define PACKET_MR_UNICAST   3

struct sockaddr_ll {
    unsigned short sll_family;
    unsigned short sll_protocol;
    int sll_ifindex;
    unsigned short sll_hatype;
    unsigned char sll_pkttype;
    unsigned char sll_halen;
    unsigned char sll_addr[8];
};

struct packet_mreq {
    int mr_ifindex;
    unsigned short mr_type;
    unsigned short mr_alen;
    unsigned char mr_address[8];
};

#endif
