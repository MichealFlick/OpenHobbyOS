#ifndef OPENHOBBYOS_SYS_SOCKET_H
#define OPENHOBBYOS_SYS_SOCKET_H

#include <sys/types.h>

typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char __ss_padding[126];
};

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_NETLINK 16
#define AF_PACKET  17
#define AF_INET6  10

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET
#define PF_NETLINK AF_NETLINK
#define PF_PACKET  AF_PACKET
#define PF_INET6  AF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3
#define SOCK_CLOEXEC 0x80000

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#define SOL_SOCKET 1
#define SO_ERROR   4
#define SO_RCVBUF  8
#define SO_RCVTIMEO 20

#define MSG_NOSIGNAL 0x4000
#define MSG_DONTWAIT 0x40
#define MSG_WAITALL  0x100
#define MSG_FASTOPEN 0x20000000

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
ssize_t send(int sockfd, const void *buffer, size_t length, int flags);
ssize_t recv(int sockfd, void *buffer, size_t length, int flags);
ssize_t sendto(int sockfd, const void *buffer, size_t length, int flags, const struct sockaddr *dest, socklen_t destlen);
ssize_t recvfrom(int sockfd, void *buffer, size_t length, int flags, struct sockaddr *src, socklen_t *srclen);
int shutdown(int sockfd, int how);

#endif
