#ifndef OPENHOBBYOS_LINUX_INPUT_H
#define OPENHOBBYOS_LINUX_INPUT_H

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/time.h>

struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define EV_MAX 0x1f

#define SYN_REPORT 0

#define REL_X      0x00
#define REL_Y      0x01
#define REL_HWHEEL 0x06
#define REL_WHEEL  0x08

#define ABS_X             0x00
#define ABS_MT_POSITION_X 0x35
#define ABS_MAX           0x3f

#define BTN_LEFT      0x110
#define BTN_MOUSE     0x110
#define BTN_JOYSTICK  0x120
#define BTN_GAMEPAD   0x130

#define KEY_MAX 0x2ff

#define EVIOCGBIT(ev, len) _IOC(_IOC_READ, 'E', 0x20 + (ev), (len))
#define EVIOCGID         _IOR('E', 0x02, struct input_id)
#define EVIOCGNAME(len)  _IOC(_IOC_READ, 'E', 0x06, (len))

#endif
