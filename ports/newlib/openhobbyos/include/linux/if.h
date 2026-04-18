#ifndef OPENHOBBYOS_LINUX_IF_H
#define OPENHOBBYOS_LINUX_IF_H

#include <net/if.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP 0x10000
#endif

#ifndef IFF_DORMANT
#define IFF_DORMANT 0x20000
#endif

#ifndef IFF_ECHO
#define IFF_ECHO 0x40000
#endif

#endif
