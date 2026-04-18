#ifndef OPENHOBBYOS_LINUX_NETLINK_H
#define OPENHOBBYOS_LINUX_NETLINK_H

#include <stdint.h>
#include <sys/socket.h>

struct sockaddr_nl {
    sa_family_t nl_family;
    unsigned short nl_pad;
    uint32_t nl_pid;
    uint32_t nl_groups;
};

struct nlmsghdr {
    uint32_t nlmsg_len;
    uint16_t nlmsg_type;
    uint16_t nlmsg_flags;
    uint32_t nlmsg_seq;
    uint32_t nlmsg_pid;
};

struct nlmsgerr {
    int error;
    struct nlmsghdr msg;
};

#define NETLINK_ROUTE 0

#define NLM_F_REQUEST 0x0001
#define NLM_F_MULTI   0x0002
#define NLM_F_ACK     0x0004
#define NLM_F_ECHO    0x0008
#define NLM_F_ROOT    0x0100
#define NLM_F_MATCH   0x0200
#define NLM_F_ATOMIC  0x0400
#define NLM_F_DUMP    (NLM_F_ROOT | NLM_F_MATCH)

#define NLMSG_NOOP   0x1
#define NLMSG_ERROR  0x2
#define NLMSG_DONE   0x3

#define NLMSG_ALIGNTO 4u
#define NLMSG_ALIGN(length) ((((uint32_t) (length)) + NLMSG_ALIGNTO - 1u) & ~(NLMSG_ALIGNTO - 1u))
#define NLMSG_HDRLEN ((uint32_t) NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_LENGTH(length) (((uint32_t) (length)) + NLMSG_HDRLEN)
#define NLMSG_SPACE(length) NLMSG_ALIGN(NLMSG_LENGTH(length))
#define NLMSG_DATA(header) ((void *) (((char *) (header)) + NLMSG_HDRLEN))
#define NLMSG_NEXT(header, length) \
    ((length) -= (int) NLMSG_ALIGN((header)->nlmsg_len), \
     (struct nlmsghdr *) (((char *) (header)) + NLMSG_ALIGN((header)->nlmsg_len)))
#define NLMSG_OK(header, length) \
    ((length) >= (int) sizeof(struct nlmsghdr) && \
     (header)->nlmsg_len >= sizeof(struct nlmsghdr) && \
     (header)->nlmsg_len <= (uint32_t) (length))
#define NLMSG_PAYLOAD(header, length) ((int) ((header)->nlmsg_len - NLMSG_SPACE(length)))

#endif
