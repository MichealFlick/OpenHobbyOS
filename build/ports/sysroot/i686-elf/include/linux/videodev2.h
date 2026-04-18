#ifndef OPENHOBBYOS_LINUX_VIDEODEV2_H
#define OPENHOBBYOS_LINUX_VIDEODEV2_H

#include <stdint.h>

#define VIDIOC_QUERYCAP 0x80685600u
#define VIDIOC_G_FMT    0xC0D05604u

#define V4L2_CAP_VIDEO_CAPTURE 1u

#define V4L2_BUF_TYPE_VIDEO_CAPTURE 1u

#define V4L2_COLORSPACE_SMPTE170M    1u
#define V4L2_COLORSPACE_SMPTE240M    2u
#define V4L2_COLORSPACE_REC709       3u
#define V4L2_COLORSPACE_BT878        4u
#define V4L2_COLORSPACE_470_SYSTEM_M 5u
#define V4L2_COLORSPACE_470_SYSTEM_BG 6u
#define V4L2_COLORSPACE_JPEG         7u
#define V4L2_COLORSPACE_SRGB         8u
#define V4L2_COLORSPACE_OPRGB        9u
#define V4L2_COLORSPACE_BT2020       10u
#define V4L2_COLORSPACE_RAW          11u
#define V4L2_COLORSPACE_DCI_P3       12u

struct v4l2_capability {
    uint8_t driver[16];
    uint8_t card[32];
    uint8_t bus_info[32];
    uint32_t version;
    uint32_t capabilities;
    uint32_t device_caps;
    uint32_t reserved[3];
};

struct v4l2_pix_format {
    uint32_t width;
    uint32_t height;
    uint32_t pixelformat;
    uint32_t field;
    uint32_t bytesperline;
    uint32_t sizeimage;
    uint32_t colorspace;
    uint32_t priv;
    uint32_t flags;
    uint32_t ycbcr_enc;
    uint32_t quantization;
    uint32_t xfer_func;
};

struct v4l2_format {
    uint32_t type;
    union {
        struct v4l2_pix_format pix;
        uint8_t raw_data[200];
    } fmt;
};

#endif
