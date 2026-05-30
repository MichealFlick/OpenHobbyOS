/* 3D gear wheels written by Brian Paul. */

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <TGL/gl.h>
#include "zbuffer.h"

#define FBIOGET_VSCREENINFO 0x4600

struct fb_var_screeninfo {
    uint32_t xres, yres, xres_virtual, yres_virtual;
    uint32_t xoffset, yoffset, bits_per_pixel, grayscale;
    struct { uint32_t offset, length, msb_right; } red, green, blue, transp;
    uint32_t pitch;
};

static uint8_t *s_FrameBuffer = NULL;
static int s_KeyboardFd = -1;
static unsigned int s_Pitch, s_BytesPerPixel, s_ScreenWidth, s_ScreenHeight;

static void flip(PIXEL *buf, int win_w, int win_h)
{
    int pos_x = ((int)s_ScreenWidth - win_w) / 2;
    int pos_y = ((int)s_ScreenHeight - win_h) / 2;
    for (int y = 0; y < win_h; y++) {
        uint8_t *dst = s_FrameBuffer + (y + pos_y) * s_Pitch + pos_x * s_BytesPerPixel;
        PIXEL *src = buf + y * win_w;
        for (int x = 0; x < win_w; x++) {
            uint32_t p = src[x];
            dst[x * s_BytesPerPixel + 0] = (uint8_t)(p >> 0);
            dst[x * s_BytesPerPixel + 1] = (uint8_t)(p >> 8);
            dst[x * s_BytesPerPixel + 2] = (uint8_t)(p >> 16);
        }
    }
}

static int check_keyboard()
{
    if (s_KeyboardFd < 0) return 0;
    unsigned char sc;
    while (read(s_KeyboardFd, &sc, 1) == 1) {
        if (!(sc & 0x80) && (sc == 0x01))
            return 1;
    }
    return 0;
}

/*
 * Draw a gear wheel.  You'll probably want to call this function when
 * building a display list since we do a lot of trig here.
 *
 * Input:  inner_radius - radius of hole at center
 *         outer_radius - radius at center of teeth
 *         width - width of gear
 *         teeth - number of teeth
 *         tooth_depth - depth of tooth
 */
static void gear(GLfloat inner_radius,
                 GLfloat outer_radius,
                 GLfloat width,
                 GLint teeth,
                 GLfloat tooth_depth)
{
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    r2 = outer_radius + tooth_depth / 2.0;

    da = 2.0 * M_PI / teeth / 4.0;

    glNormal3f(0.0, 0.0, 1.0);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (GLint i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                   width * 0.5);
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0 * M_PI / teeth / 4.0;
    for (GLint i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                   width * 0.5);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                   width * 0.5);
    }
    glEnd();

    glNormal3f(0.0, 0.0, -1.0);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (GLint i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                   -width * 0.5);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0 * M_PI / teeth / 4.0;
    for (GLint i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                   -width * 0.5);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                   -width * 0.5);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (GLint i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
        u = r2 * cos(angle + da) - r1 * cos(angle);
        v = r2 * sin(angle + da) - r1 * sin(angle);
        len = sqrt(u * u + v * v);
        u /= len;
        v /= len;
        glNormal3f(v, -u, 0.0);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
        glNormal3f(cos(angle), sin(angle), 0.0);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                   width * 0.5);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                   -width * 0.5);
        u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
        v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
        glNormal3f(v, -u, 0.0);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                   width * 0.5);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                   -width * 0.5);
        glNormal3f(cos(angle), sin(angle), 0.0);
    }

    glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
    glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

    glEnd();

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (GLint i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glNormal3f(-cos(angle), -sin(angle), 0.0);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    }
    glEnd();
}

static GLfloat view_rotx = 20.0, view_roty = 30.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static void draw()
{
    angle += 2.0;
    glPushMatrix();
    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);

    glPushMatrix();
    glTranslatef(-3.0, -2.0, 0.0);
    glRotatef(angle, 0.0, 0.0, 1.0);
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1, -2.0, 0.0);
    glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1, 4.2, 0.0);
    glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
    glCallList(gear3);
    glPopMatrix();

    glPopMatrix();
}

static void init_scene()
{
    static GLfloat pos[4] = {5, 5, 10, 0.0};

    static GLfloat red[4] = {1.0, 0.0, 0.0, 0.0};
    static GLfloat green[4] = {0.0, 1.0, 0.0, 0.0};
    static GLfloat blue[4] = {0.0, 0.0, 1.0, 0.0};
    static GLfloat white[4] = {1.0, 1.0, 1.0, 0.0};
    static GLfloat shininess = 5;

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    glEnable(GL_CULL_FACE);

    glEnable(GL_LIGHT0);

    glPointSize(10.0f);

    gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, blue);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glMaterialfv(GL_FRONT, GL_SHININESS, &shininess);
    glColor3fv(blue);
    gear(1.0, 4.0, 1.0, 20, 0.7);
    glEndList();

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, red);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glColor3fv(red);
    gear(0.5, 2.0, 2.0, 10, 0.7);
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, green);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glColor3fv(green);
    gear(1.3, 2.0, 0.5, 10, 0.7);
    glEndList();
}

int main(int argc, char **argv)
{
    int win_w = 320, win_h = 240;

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { fprintf(stderr, "gears: no fb\n"); return 1; }

    struct fb_var_screeninfo vi;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vi);
    s_ScreenWidth = vi.xres; s_ScreenHeight = vi.yres;
    s_BytesPerPixel = (vi.bits_per_pixel + 7u) / 8u;
    s_Pitch = vi.pitch;

    unsigned int fb_sz = s_Pitch * s_ScreenHeight;
    s_FrameBuffer = (uint8_t *)mmap(NULL, fb_sz, PROT_READ|PROT_WRITE, MAP_SHARED, fb_fd, 0);
    close(fb_fd);
    if (s_FrameBuffer == MAP_FAILED) { fprintf(stderr, "gears: mmap failed\n"); return 1; }

    s_KeyboardFd = open("/dev/keyboard", O_RDONLY);

    PIXEL *imbuf = calloc(1, sizeof(PIXEL) * win_w * win_h);

    ZBuffer *frameBuffer = ZB_open(win_w, win_h, ZB_MODE_RGBA, imbuf);
    if (!frameBuffer) { printf("ZB_open failed\n"); return 1; }
    glInit(frameBuffer);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, win_w, win_h);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    GLfloat h = (GLfloat)win_h / win_w;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -45.0);

    init_scene();
    glSetEnableSpecular(GL_TRUE);

    int frame = 0;
    while (1) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw();

        flip(imbuf, win_w, win_h);
        frame++;

        if (check_keyboard()) break;
        usleep(16000);
        if ((frame % 60) == 0) printf("frame %d\n", frame);
    }

    glDeleteList(gear1);
    glDeleteList(gear2);
    glDeleteList(gear3);

    ZB_close(frameBuffer);
    glClose();
    free(imbuf);
    munmap(s_FrameBuffer, fb_sz);
    if (s_KeyboardFd >= 0) close(s_KeyboardFd);
    printf("done: %d frames\n", frame);
    return 0;
}
