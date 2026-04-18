#ifndef OPENHOBBYOS_NET_IF_H
#define OPENHOBBYOS_NET_IF_H

#include <sys/socket.h>

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

#define IFF_UP          0x0001
#define IFF_BROADCAST   0x0002
#define IFF_DEBUG       0x0004
#define IFF_LOOPBACK    0x0008
#define IFF_POINTOPOINT 0x0010
#define IFF_RUNNING     0x0040
#define IFF_NOARP       0x0080
#define IFF_PROMISC     0x0100
#define IFF_ALLMULTI    0x0200
#define IFF_MASTER      0x0400
#define IFF_SLAVE       0x0800
#define IFF_MULTICAST   0x1000
#define IFF_PORTSEL     0x2000
#define IFF_AUTOMEDIA   0x4000
#define IFF_DYNAMIC     0x8000

#define SIOCGIFCONF     0x8912
#define SIOCGIFFLAGS    0x8913
#define SIOCGIFBRDADDR  0x8919

struct if_nameindex {
    unsigned int if_index;
    char *if_name;
};

struct ifconf;

struct ifreq {
    char ifr_name[IFNAMSIZ];
    union {
        struct sockaddr ifru_addr;
        struct sockaddr ifru_broadaddr;
        struct sockaddr ifru_hwaddr;
        short ifru_flags;
        int ifru_ifindex;
        int ifru_mtu;
        struct ifconf *ifru_ifconf;
        void *ifru_data;
    } ifr_ifru;
};

#define ifr_addr    ifr_ifru.ifru_addr
#define ifr_broadaddr ifr_ifru.ifru_broadaddr
#define ifr_hwaddr  ifr_ifru.ifru_hwaddr
#define ifr_flags   ifr_ifru.ifru_flags
#define ifr_ifindex ifr_ifru.ifru_ifindex
#define ifr_mtu     ifr_ifru.ifru_mtu
#define ifr_data    ifr_ifru.ifru_data

struct ifconf {
    int ifc_len;
    union {
        char *ifcu_buf;
        struct ifreq *ifcu_req;
    } ifc_ifcu;
};

#define ifc_buf ifc_ifcu.ifcu_buf
#define ifc_req ifc_ifcu.ifcu_req

unsigned int if_nametoindex(const char *ifname);
char *if_indextoname(unsigned int ifindex, char *ifname);
struct if_nameindex *if_nameindex(void);
void if_freenameindex(struct if_nameindex *ptr);

#endif
