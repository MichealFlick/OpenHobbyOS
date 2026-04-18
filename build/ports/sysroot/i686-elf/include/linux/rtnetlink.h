#ifndef OPENHOBBYOS_LINUX_RTNETLINK_H
#define OPENHOBBYOS_LINUX_RTNETLINK_H

#include <linux/netlink.h>

struct rtmsg {
    unsigned char rtm_family;
    unsigned char rtm_dst_len;
    unsigned char rtm_src_len;
    unsigned char rtm_tos;
    unsigned char rtm_table;
    unsigned char rtm_protocol;
    unsigned char rtm_scope;
    unsigned char rtm_type;
    unsigned int rtm_flags;
};

struct rtattr {
    unsigned short rta_len;
    unsigned short rta_type;
};

#define RTM_NEWROUTE 24
#define RTM_GETROUTE 26

#define RT_TABLE_UNSPEC 0
#define RT_TABLE_MAIN 254

#define RTPROT_UNSPEC 0

#define RT_SCOPE_UNIVERSE 0
#define RT_SCOPE_HOST 254

#define RTN_UNSPEC 0
#define RTN_LOCAL 2

#define RTA_UNSPEC 0
#define RTA_DST 1
#define RTA_OIF 4
#define RTA_GATEWAY 5
#define RTA_PRIORITY 6
#define RTA_PREFSRC 7
#define RTA_TABLE 15

#define RTA_ALIGNTO 4u
#define RTA_ALIGN(length) ((((uint32_t) (length)) + RTA_ALIGNTO - 1u) & ~(RTA_ALIGNTO - 1u))
#define RTA_LENGTH(length) ((unsigned short) (RTA_ALIGN(sizeof(struct rtattr)) + ((uint32_t) (length))))
#define RTA_SPACE(length) RTA_ALIGN(RTA_LENGTH(length))
#define RTA_DATA(attribute) ((void *) (((char *) (attribute)) + RTA_ALIGN(sizeof(struct rtattr))))
#define RTA_PAYLOAD(attribute) ((int) ((attribute)->rta_len - (unsigned short) RTA_ALIGN(sizeof(struct rtattr))))
#define RTA_NEXT(attribute, length) \
    ((length) -= (int) RTA_ALIGN((attribute)->rta_len), \
     (struct rtattr *) (((char *) (attribute)) + RTA_ALIGN((attribute)->rta_len)))
#define RTA_OK(attribute, length) \
    ((length) >= (int) sizeof(struct rtattr) && \
     (attribute)->rta_len >= sizeof(struct rtattr) && \
     (attribute)->rta_len <= (unsigned short) (length))

#define RTM_RTA(message) \
    ((struct rtattr *) (((char *) (message)) + NLMSG_ALIGN(sizeof(struct rtmsg))))
#define RTM_PAYLOAD(header) NLMSG_PAYLOAD((header), sizeof(struct rtmsg))

#endif
