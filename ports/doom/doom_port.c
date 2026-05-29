#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define FBIOGET_VSCREENINFO 0x4600

struct fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
    uint32_t pitch;
};

static int FrameBufferFd = -1;
static uint8_t *FrameBuffer = NULL;
static int KeyboardFd = -1;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned int s_ScreenWidth = 0;
static unsigned int s_ScreenHeight = 0;
static unsigned int s_Pitch = 0;
static unsigned int s_BytesPerPixel = 0;

static int s_PosX = 0;
static int s_PosY = 0;
static int s_CtrlPressed = 0;

static unsigned char convertToDoomKey(unsigned char scancode) {
    switch (scancode) {
    case 0x1C: case 0x9C: return KEY_ENTER;
    case 0x01: return KEY_ESCAPE;
    case 0x4B: case 0xCB: return KEY_LEFTARROW;
    case 0x4D: case 0xCD: return KEY_RIGHTARROW;
    case 0x48: case 0xC8: return KEY_UPARROW;
    case 0x50: case 0xD0: return KEY_DOWNARROW;
    case 0x1D: return KEY_FIRE;
    case 0x39: return KEY_USE;
    case 0x2A: case 0x36: return KEY_RSHIFT;
    case 0x15: return 'y';
    default: return 0;
    }
}

static void addKeyToQueue(int pressed, unsigned char keyCode) {
    unsigned char key = convertToDoomKey(keyCode);
    if (key == 0) return;
    unsigned short keyData = (pressed << 8) | key;
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex = (s_KeyQueueWriteIndex + 1) % KEYQUEUE_SIZE;
}

void DG_Init() {
    FrameBufferFd = open("/dev/fb0", O_RDWR);
    if (FrameBufferFd < 0) {
        fprintf(stderr, "doom: failed to open /dev/fb0\n");
        exit(1);
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(FrameBufferFd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "doom: FBIOGET_VSCREENINFO failed\n");
        exit(1);
    }

    s_ScreenWidth = vinfo.xres;
    s_ScreenHeight = vinfo.yres;
    s_BytesPerPixel = (vinfo.bits_per_pixel + 7u) / 8u;
    s_Pitch = vinfo.pitch;

    printf("doom: framebuffer %dx%d %dbpp pitch=%u\n",
           s_ScreenWidth, s_ScreenHeight, vinfo.bits_per_pixel, s_Pitch);

    unsigned int fb_size = s_Pitch * s_ScreenHeight;
    FrameBuffer = (uint8_t *)mmap(NULL, fb_size,
                                   PROT_READ | PROT_WRITE, MAP_SHARED,
                                   FrameBufferFd, 0);
    if (FrameBuffer == MAP_FAILED) {
        fprintf(stderr, "doom: mmap framebuffer failed\n");
        exit(1);
    }

    KeyboardFd = open("/dev/keyboard", O_RDONLY);
    if (KeyboardFd < 0) {
        fprintf(stderr, "doom: failed to open /dev/keyboard\n");
    }

    s_PosX = (s_ScreenWidth - DOOMGENERIC_RESX) / 2;
    s_PosY = (s_ScreenHeight - DOOMGENERIC_RESY) / 2;
}

static void handleKeyInput() {
    if (KeyboardFd < 0) return;

    unsigned char scancode = 0;
    while (read(KeyboardFd, &scancode, 1) == 1) {
        unsigned char released = (scancode & 0x80) != 0;
        unsigned char key = scancode & 0x7F;

        if (key == 0x1D) {
            s_CtrlPressed = !released;
        }

        if (key == 0x2E && !released && s_CtrlPressed) {
            printf("doom: Ctrl+C pressed, exiting\n");
            exit(0);
        }

        addKeyToQueue(!released, key);
    }
}

void DG_DrawFrame() {
    if (FrameBuffer) {
        for (int i = 0; i < DOOMGENERIC_RESY; i++) {
            uint8_t *dst = FrameBuffer + (i + s_PosY) * s_Pitch + s_PosX * s_BytesPerPixel;
            uint32_t *src = DG_ScreenBuffer + i * DOOMGENERIC_RESX;
            for (int j = 0; j < DOOMGENERIC_RESX; j++) {
                uint32_t pixel = src[j];
                dst[j * s_BytesPerPixel + 0] = (uint8_t)(pixel >> 0);
                dst[j * s_BytesPerPixel + 1] = (uint8_t)(pixel >> 8);
                dst[j * s_BytesPerPixel + 2] = (uint8_t)(pixel >> 16);
                if (s_BytesPerPixel > 3)
                    dst[j * s_BytesPerPixel + 3] = (uint8_t)(pixel >> 24);
            }
        }
    }
    handleKeyInput();
}

void DG_SleepMs(uint32_t ms) {
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) return 0;

    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex = (s_KeyQueueReadIndex + 1) % KEYQUEUE_SIZE;
    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char *title) {
    (void)title;
}

int main(int argc, char **argv) {
    doomgeneric_Create(argc, argv);
    while (1) {
        doomgeneric_Tick();
    }
    return 0;
}
