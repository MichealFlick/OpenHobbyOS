#ifndef OPENHOBBYOS_LINUX_WIRELESS_H
#define OPENHOBBYOS_LINUX_WIRELESS_H

#include <net/if.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#define IW_ESSID_MAX_SIZE 32

#define SIOCGIWNAME      0x8B01
#define SIOCGIWFREQ      0x8B05
#define SIOCGIWSTATS     0x8B0F
#define SIOCGIWAP        0x8B15
#define SIOCGIWESSID     0x8B1B
#define SIOCGIWRATE      0x8B21
#define SIOCGIWENCODEEXT 0x8B35

#define IW_ENCODE_ALG_NONE     0
#define IW_ENCODE_ALG_WEP      1
#define IW_ENCODE_ALG_TKIP     2
#define IW_ENCODE_ALG_CCMP     3
#define IW_ENCODE_ALG_PMK      4
#define IW_ENCODE_ALG_AES_CMAC 5

struct iw_param {
    int32_t value;
    uint8_t fixed;
    uint8_t disabled;
    uint16_t flags;
};

struct iw_point {
    void *pointer;
    uint16_t length;
    uint16_t flags;
};

struct iw_freq {
    int32_t m;
    int16_t e;
    uint8_t i;
    uint8_t flags;
};

struct iw_quality {
    uint8_t qual;
    uint8_t level;
    uint8_t noise;
    uint8_t updated;
};

struct iw_statistics {
    uint16_t status;
    struct iw_quality qual;
};

struct iw_encode_ext {
    uint32_t ext_flags;
    uint8_t tx_seq[8];
    uint8_t rx_seq[8];
    struct sockaddr addr;
    uint16_t alg;
    uint16_t key_len;
    uint8_t key[32];
};

union iwreq_data {
    char name[IFNAMSIZ];
    struct iw_point essid;
    struct iw_point data;
    struct sockaddr ap_addr;
    struct iw_param bitrate;
    struct iw_freq freq;
};

struct iwreq {
    union {
        char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;
    union iwreq_data u;
};

#define ifr_name ifr_ifrn.ifrn_name

#endif
