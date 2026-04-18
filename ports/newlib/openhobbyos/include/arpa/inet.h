#ifndef OPENHOBBYOS_ARPA_INET_H
#define OPENHOBBYOS_ARPA_INET_H

#include <stddef.h>

#include <netinet/in.h>

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

#define htonl(x) __builtin_bswap32((uint32_t) (x))
#define htons(x) __builtin_bswap16((uint16_t) (x))
#define ntohl(x) __builtin_bswap32((uint32_t) (x))
#define ntohs(x) __builtin_bswap16((uint16_t) (x))

const char *inet_ntop(int family, const void *src, char *dst, socklen_t size);
int inet_pton(int family, const char *src, void *dst);
char *inet_ntoa(struct in_addr in);

#endif
