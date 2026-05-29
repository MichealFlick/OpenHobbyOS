#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0

#define MEM_ALIGNMENT                   4
#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1

#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_DHCP                       1
#define LWIP_AUTOIP                     0
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_DNS                        1
#define LWIP_STATS                      0
#define LWIP_HAVE_LOOPIF                0
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define LWIP_SNMP                       0
#define LWIP_IGMP                       0
#define PPP_SUPPORT                     0
#define IP_FORWARD                      0
#define IP_REASSEMBLY                   0
#define IP_FRAG                         0
#define LWIP_COMPAT_MUTEX               0
#define LWIP_TIMERS                     1

#define TCP_LISTEN_BACKLOG              0
#define TCP_MSS                         1460
#define TCP_WND                         65535
#define TCP_SND_BUF                     65535

#define MEMP_NUM_PBUF                   32
#define MEMP_NUM_RAW_PCB                4
#define MEMP_NUM_UDP_PCB                8
#define MEMP_NUM_TCP_PCB                8
#define MEMP_NUM_TCP_PCB_LISTEN         4
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_ARP_QUEUE              8

#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               512

#define LWIP_DNS_SECURE                 0

#define LWIP_DEBUG                     0

#endif
