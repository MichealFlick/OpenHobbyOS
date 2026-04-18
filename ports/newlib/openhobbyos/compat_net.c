#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
int h_errno;

static int oh_parse_ipv4(const char *src, struct in_addr *addr) {
    unsigned long parts[4];
    char tail;

    if (sscanf(src, "%lu.%lu.%lu.%lu%c", &parts[0], &parts[1], &parts[2], &parts[3], &tail) != 4) {
        return 0;
    }
    for (int i = 0; i < 4; ++i) {
        if (parts[i] > 255u) {
            return 0;
        }
    }

    addr->s_addr = htonl((uint32_t) ((parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]));
    return 1;
}

static int oh_parse_ipv6(const char *src, struct in6_addr *addr) {
    unsigned short words[8];
    unsigned int used = 0;
    int compress = -1;
    const char *cursor = src;

    memset(addr, 0, sizeof(*addr));

    if (*cursor == ':') {
        if (cursor[1] != ':') {
            return 0;
        }
        compress = 0;
        cursor += 2;
    }

    while (*cursor) {
        unsigned int value = 0;
        int digits = 0;

        if (used >= 8) {
            return 0;
        }

        while (*cursor) {
            char ch = *cursor;
            unsigned int nibble;

            if (ch >= '0' && ch <= '9') {
                nibble = (unsigned int) (ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                nibble = (unsigned int) (ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                nibble = (unsigned int) (ch - 'A' + 10);
            } else {
                break;
            }

            value = (value << 4) | nibble;
            digits++;
            if (digits > 4) {
                return 0;
            }
            cursor++;
        }

        if (digits == 0) {
            return 0;
        }

        words[used++] = (unsigned short) value;

        if (*cursor == '\0') {
            break;
        }
        if (*cursor != ':') {
            return 0;
        }
        if (cursor[1] == ':') {
            if (compress >= 0) {
                return 0;
            }
            compress = (int) used;
            cursor += 2;
            if (*cursor == '\0') {
                break;
            }
        } else {
            cursor++;
        }
    }

    if (compress >= 0) {
        unsigned int tail = used - (unsigned int) compress;
        for (int i = 7; i >= compress + (int) tail; --i) {
            words[i] = words[i - (8 - used)];
        }
        for (int i = compress; i < 8 - (int) tail; ++i) {
            words[i] = 0;
        }
        used = 8;
    }

    if (used != 8) {
        return 0;
    }

    for (int i = 0; i < 8; ++i) {
        addr->s6_addr[i * 2] = (unsigned char) (words[i] >> 8);
        addr->s6_addr[i * 2 + 1] = (unsigned char) words[i];
    }

    return 1;
}

const char *inet_ntop(int family, const void *src, char *dst, socklen_t size) {
    if (family == AF_INET) {
        const unsigned char *bytes = (const unsigned char *) src;
        uint32_t value = ntohl(*(const uint32_t *) bytes);
        int written = snprintf(dst, size, "%u.%u.%u.%u",
                               (unsigned int) ((value >> 24) & 0xffu),
                               (unsigned int) ((value >> 16) & 0xffu),
                               (unsigned int) ((value >> 8) & 0xffu),
                               (unsigned int) (value & 0xffu));
        if (written < 0 || (socklen_t) written >= size) {
            errno = ENOSPC;
            return NULL;
        }
        return dst;
    }

    if (family == AF_INET6) {
        const struct in6_addr *addr = (const struct in6_addr *) src;
        unsigned short words[8];
        char buffer[INET6_ADDRSTRLEN];
        char *cursor = buffer;
        int best_start = -1;
        int best_len = 0;

        for (int i = 0; i < 8; ++i) {
            words[i] = (unsigned short) (((unsigned int) addr->s6_addr[i * 2] << 8) | addr->s6_addr[i * 2 + 1]);
        }

        for (int i = 0; i < 8; ) {
            int j = i;
            while (j < 8 && words[j] == 0) {
                j++;
            }
            if (j - i > best_len) {
                best_start = i;
                best_len = j - i;
            }
            i = (j == i) ? (i + 1) : j;
        }

        if (best_len < 2) {
            best_start = -1;
            best_len = 0;
        }

        for (int i = 0; i < 8; ++i) {
            if (i == best_start) {
                *cursor++ = ':';
                *cursor++ = ':';
                i += best_len - 1;
                continue;
            }

            if (i != 0 && cursor[-1] != ':') {
                *cursor++ = ':';
            }

            cursor += sprintf(cursor, "%x", words[i]);
        }

        *cursor = '\0';
        if ((socklen_t) (cursor - buffer + 1) > size) {
            errno = ENOSPC;
            return NULL;
        }
        memcpy(dst, buffer, (size_t) (cursor - buffer + 1));
        return dst;
    }

    errno = EAFNOSUPPORT;
    return NULL;
}

int inet_pton(int family, const char *src, void *dst) {
    if (family == AF_INET) {
        return oh_parse_ipv4(src, (struct in_addr *) dst);
    }
    if (family == AF_INET6) {
        return oh_parse_ipv6(src, (struct in6_addr *) dst);
    }

    errno = EAFNOSUPPORT;
    return -1;
}

char *inet_ntoa(struct in_addr in) {
    static char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in, buffer, sizeof(buffer));
    return buffer;
}

unsigned int if_nametoindex(const char *ifname) {
    return (ifname && strcmp(ifname, "lo") == 0) ? 1u : 0u;
}

char *if_indextoname(unsigned int ifindex, char *ifname) {
    if (ifname == NULL || ifindex != 1u) {
        errno = ENXIO;
        return NULL;
    }

    strcpy(ifname, "lo");
    return ifname;
}

struct if_nameindex *if_nameindex(void) {
    struct if_nameindex *list = (struct if_nameindex *) calloc(2, sizeof(*list));

    if (list == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    list[0].if_index = 1u;
    list[0].if_name = strdup("lo");
    if (list[0].if_name == NULL) {
        free(list);
        errno = ENOMEM;
        return NULL;
    }

    return list;
}

void if_freenameindex(struct if_nameindex *ptr) {
    if (ptr == NULL) {
        return;
    }

    free(ptr[0].if_name);
    free(ptr);
}

struct hostent *gethostbyname(const char *name) {
    static struct hostent host;
    static char *aliases[1];
    static char *addr_list[2];
    static char canonical[256];
    static unsigned char addr_bytes[sizeof(struct in_addr)];
    struct utsname uts;
    struct in_addr addr;

    if (name == NULL || *name == '\0') {
        h_errno = HOST_NOT_FOUND;
        errno = EINVAL;
        return NULL;
    }

    if (strcmp(name, "localhost") != 0) {
        if (gethostname(canonical, sizeof(canonical)) == 0) {
            if (strcmp(name, canonical) != 0) {
                if (uname(&uts) != 0 || strcmp(name, uts.nodename) != 0) {
                    if (!oh_parse_ipv4(name, &addr)) {
                        h_errno = HOST_NOT_FOUND;
                        errno = ENOENT;
                        return NULL;
                    }
                } else {
                    addr.s_addr = htonl(INADDR_LOOPBACK);
                }
            } else {
                addr.s_addr = htonl(INADDR_LOOPBACK);
            }
        } else if (!oh_parse_ipv4(name, &addr)) {
            h_errno = HOST_NOT_FOUND;
            errno = ENOENT;
            return NULL;
        }
    } else {
        addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    strncpy(canonical, name, sizeof(canonical) - 1);
    canonical[sizeof(canonical) - 1] = '\0';
    memcpy(addr_bytes, &addr, sizeof(addr));

    aliases[0] = NULL;
    addr_list[0] = (char *) addr_bytes;
    addr_list[1] = NULL;
    host.h_name = canonical;
    host.h_aliases = aliases;
    host.h_addrtype = AF_INET;
    host.h_length = (int) sizeof(addr_bytes);
    host.h_addr_list = addr_list;
    h_errno = 0;
    return &host;
}

struct servent *getservbyname(const char *name, const char *proto) {
    static struct servent service;
    static char *aliases[1];
    static char service_name[32];
    static char service_proto[16];

    if (name == NULL || proto == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (strcmp(proto, "tcp") != 0 && strcmp(proto, "udp") != 0) {
        errno = ENOENT;
        return NULL;
    }
    if (strcmp(name, "x11") != 0) {
        errno = ENOENT;
        return NULL;
    }

    strncpy(service_name, name, sizeof(service_name) - 1);
    service_name[sizeof(service_name) - 1] = '\0';
    strncpy(service_proto, proto, sizeof(service_proto) - 1);
    service_proto[sizeof(service_proto) - 1] = '\0';

    aliases[0] = NULL;
    service.s_name = service_name;
    service.s_aliases = aliases;
    service.s_port = htons(6000);
    service.s_proto = service_proto;
    return &service;
}

static struct ifaddrs *oh_add_ifaddr(struct ifaddrs **tail, int family) {
    struct ifaddrs *ifa = (struct ifaddrs *) calloc(1, sizeof(*ifa));

    if (ifa == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    ifa->ifa_name = strdup("lo");
    if (ifa->ifa_name == NULL) {
        free(ifa);
        errno = ENOMEM;
        return NULL;
    }
    ifa->ifa_flags = IFF_UP | IFF_RUNNING | IFF_LOOPBACK;

    if (family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in *) calloc(1, sizeof(*addr));
        struct sockaddr_in *mask = (struct sockaddr_in *) calloc(1, sizeof(*mask));

        if (addr == NULL || mask == NULL) {
            free(addr);
            free(mask);
            free(ifa->ifa_name);
            free(ifa);
            errno = ENOMEM;
            return NULL;
        }

        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        mask->sin_family = AF_INET;
        mask->sin_addr.s_addr = htonl(0xff000000u);
        ifa->ifa_addr = (struct sockaddr *) addr;
        ifa->ifa_netmask = (struct sockaddr *) mask;
    } else {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *) calloc(1, sizeof(*addr));
        struct sockaddr_in6 *mask = (struct sockaddr_in6 *) calloc(1, sizeof(*mask));

        if (addr == NULL || mask == NULL) {
            free(addr);
            free(mask);
            free(ifa->ifa_name);
            free(ifa);
            errno = ENOMEM;
            return NULL;
        }

        addr->sin6_family = AF_INET6;
        addr->sin6_addr = in6addr_loopback;
        mask->sin6_family = AF_INET6;
        memset(&mask->sin6_addr, 0xff, sizeof(mask->sin6_addr));
        ifa->ifa_addr = (struct sockaddr *) addr;
        ifa->ifa_netmask = (struct sockaddr *) mask;
    }

    if (*tail) {
        (*tail)->ifa_next = ifa;
    }
    *tail = ifa;
    return ifa;
}

int getifaddrs(struct ifaddrs **ifap) {
    struct ifaddrs *head = NULL;
    struct ifaddrs *tail = NULL;

    if (ifap == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (oh_add_ifaddr(&tail, AF_INET) == NULL) {
        return -1;
    }
    head = tail;

    if (oh_add_ifaddr(&tail, AF_INET6) == NULL) {
        freeifaddrs(head);
        return -1;
    }

    *ifap = head;
    return 0;
}

void freeifaddrs(struct ifaddrs *ifa) {
    while (ifa) {
        struct ifaddrs *next = ifa->ifa_next;
        free(ifa->ifa_name);
        free(ifa->ifa_addr);
        free(ifa->ifa_netmask);
        free(ifa);
        ifa = next;
    }
}

static int oh_addrinfo_set_v4(struct addrinfo **result, const char *node, unsigned long port) {
    struct addrinfo *info = (struct addrinfo *) calloc(1, sizeof(*info));
    struct sockaddr_in *addr = (struct sockaddr_in *) calloc(1, sizeof(*addr));

    if (info == NULL || addr == NULL) {
        free(info);
        free(addr);
        return EAI_MEMORY;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t) port);
    if (node == NULL || strcmp(node, "localhost") == 0) {
        addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (!oh_parse_ipv4(node, &addr->sin_addr)) {
        free(info);
        free(addr);
        return EAI_NONAME;
    }

    info->ai_family = AF_INET;
    info->ai_socktype = SOCK_STREAM;
    info->ai_protocol = IPPROTO_TCP;
    info->ai_addrlen = sizeof(*addr);
    info->ai_addr = (struct sockaddr *) addr;
    info->ai_canonname = node ? strdup(node) : NULL;
    *result = info;
    return 0;
}

static int oh_addrinfo_set_v6(struct addrinfo **result, const char *node, unsigned long port) {
    struct addrinfo *info = (struct addrinfo *) calloc(1, sizeof(*info));
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *) calloc(1, sizeof(*addr));

    if (info == NULL || addr == NULL) {
        free(info);
        free(addr);
        return EAI_MEMORY;
    }

    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons((uint16_t) port);
    if (node == NULL || strcmp(node, "localhost") == 0 || strcmp(node, "::1") == 0) {
        addr->sin6_addr = in6addr_loopback;
    } else if (!oh_parse_ipv6(node, &addr->sin6_addr)) {
        free(info);
        free(addr);
        return EAI_NONAME;
    }

    info->ai_family = AF_INET6;
    info->ai_socktype = SOCK_STREAM;
    info->ai_protocol = IPPROTO_TCP;
    info->ai_addrlen = sizeof(*addr);
    info->ai_addr = (struct sockaddr *) addr;
    info->ai_canonname = node ? strdup(node) : NULL;
    *result = info;
    return 0;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    unsigned long port = 0;
    int family = AF_UNSPEC;

    if (result == NULL) {
        return EAI_FAIL;
    }
    *result = NULL;

    if (service && *service) {
        char *end = NULL;
        port = strtoul(service, &end, 10);
        if (end == service || *end != '\0' || port > 65535u) {
            return EAI_SERVICE;
        }
    }

    if (hints) {
        family = hints->ai_family;
    }

    if (family == AF_UNSPEC || family == AF_INET) {
        int status = oh_addrinfo_set_v4(result, node, port);
        if (status == 0 || family == AF_INET) {
            return status;
        }
    }

    if (family == AF_UNSPEC || family == AF_INET6) {
        return oh_addrinfo_set_v6(result, node, port);
    }

    return EAI_FAMILY;
}

void freeaddrinfo(struct addrinfo *result) {
    while (result) {
        struct addrinfo *next = result->ai_next;
        free(result->ai_addr);
        free(result->ai_canonname);
        free(result);
        result = next;
    }
}

const char *gai_strerror(int errcode) {
    switch (errcode) {
        case 0:
            return "success";
        case EAI_BADFLAGS:
            return "bad flags";
        case EAI_NONAME:
            return "name not known";
        case EAI_AGAIN:
            return "temporary failure";
        case EAI_FAIL:
            return "permanent failure";
        case EAI_FAMILY:
            return "unsupported family";
        case EAI_SOCKTYPE:
            return "unsupported socket type";
        case EAI_SERVICE:
            return "service not available";
        case EAI_MEMORY:
            return "out of memory";
        case EAI_SYSTEM:
            return "system error";
        default:
            return "unknown error";
    }
}

int poll(struct pollfd *fds, nfds_t count, int timeout) {
    int ready = 0;

    for (nfds_t i = 0; i < count; ++i) {
        struct stat st;

        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            continue;
        }

        if (fstat(fds[i].fd, &st) == 0 || isatty(fds[i].fd)) {
            if (fds[i].events & POLLIN) {
                fds[i].revents |= POLLIN;
            }
            if (fds[i].events & POLLOUT) {
                fds[i].revents |= POLLOUT;
            }
            if (fds[i].revents) {
                ready++;
            }
        } else {
            fds[i].revents |= POLLNVAL;
            ready++;
        }
    }

    if (ready == 0 && timeout > 0) {
        struct timespec wait_time;
        wait_time.tv_sec = timeout / 1000;
        wait_time.tv_nsec = (long) ((timeout % 1000) * 1000000);
        nanosleep(&wait_time, NULL);
    }

    return ready;
}

int socket(int domain, int type, int protocol) {
    (void) domain;
    (void) type;
    (void) protocol;
    errno = ENOSYS;
    return -1;
}

int listen(int sockfd, int backlog) {
    (void) sockfd;
    (void) backlog;
    errno = ENOSYS;
    return -1;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void) sockfd;
    (void) addr;
    (void) addrlen;
    errno = ENOSYS;
    return -1;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void) sockfd;
    (void) addr;
    (void) addrlen;
    errno = ENOSYS;
    return -1;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void) sockfd;
    (void) addr;
    (void) addrlen;
    errno = ENOSYS;
    return -1;
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void) sockfd;
    (void) addr;
    (void) addrlen;
    errno = ENOSYS;
    return -1;
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void) sockfd;
    (void) addr;
    (void) addrlen;
    errno = ENOSYS;
    return -1;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    (void) sockfd;
    (void) level;
    (void) optname;
    (void) optval;
    (void) optlen;
    errno = ENOSYS;
    return -1;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    (void) sockfd;
    (void) level;
    (void) optname;
    (void) optval;
    (void) optlen;
    errno = ENOSYS;
    return -1;
}

ssize_t send(int sockfd, const void *buffer, size_t length, int flags) {
    (void) sockfd;
    (void) buffer;
    (void) length;
    (void) flags;
    errno = ENOSYS;
    return -1;
}

ssize_t recv(int sockfd, void *buffer, size_t length, int flags) {
    (void) sockfd;
    (void) buffer;
    (void) length;
    (void) flags;
    errno = ENOSYS;
    return -1;
}

ssize_t sendto(int sockfd, const void *buffer, size_t length, int flags, const struct sockaddr *dest, socklen_t destlen) {
    (void) sockfd;
    (void) buffer;
    (void) length;
    (void) flags;
    (void) dest;
    (void) destlen;
    errno = ENOSYS;
    return -1;
}

ssize_t recvfrom(int sockfd, void *buffer, size_t length, int flags, struct sockaddr *src, socklen_t *srclen) {
    (void) sockfd;
    (void) buffer;
    (void) length;
    (void) flags;
    (void) src;
    (void) srclen;
    errno = ENOSYS;
    return -1;
}

int shutdown(int sockfd, int how) {
    (void) sockfd;
    (void) how;
    errno = ENOSYS;
    return -1;
}
