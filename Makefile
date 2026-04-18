PROJECT := OpenHobbyOS
BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.bin
INITRD := $(BUILD_DIR)/initrd.bin
ISO := $(BUILD_DIR)/$(PROJECT).iso
PORTS_DIR := $(BUILD_DIR)/ports
PORTS_SYSROOT := $(PORTS_DIR)/sysroot
NEWLIB_PORT_FILES := $(shell find ports/newlib/openhobbyos -type f | sort)
XORGPROTO_PC := $(PORTS_SYSROOT)/share/pkgconfig/xproto.pc
XTRANS_PC := $(PORTS_SYSROOT)/lib/pkgconfig/xtrans.pc
ZLIB_PC := $(PORTS_SYSROOT)/lib/pkgconfig/zlib.pc
LIBSHA1_PC := $(PORTS_SYSROOT)/lib/pkgconfig/libsha1.pc
XAU_PC := $(PORTS_SYSROOT)/lib/pkgconfig/xau.pc
FONTENC_PC := $(PORTS_SYSROOT)/lib/pkgconfig/fontenc.pc
X11_PC := $(PORTS_SYSROOT)/lib/pkgconfig/x11.pc
PIXMAN_PC := $(PORTS_SYSROOT)/lib/pkgconfig/pixman-1.pc
XKBFILE_PC := $(PORTS_SYSROOT)/lib/pkgconfig/xkbfile.pc
XFONT2_PC := $(PORTS_SYSROOT)/lib/pkgconfig/xfont2.pc
XSERVER_DEPS := $(XORGPROTO_PC) $(XTRANS_PC) $(ZLIB_PC) $(LIBSHA1_PC) $(XAU_PC) $(FONTENC_PC) $(X11_PC) $(PIXMAN_PC) $(XKBFILE_PC) $(XFONT2_PC)
FASTFETCH_BIN := $(PORTS_DIR)/fastfetch/install/usr/bin/fastfetch
XSERVER_BIN := $(PORTS_DIR)/xserver/install/usr/bin/Xvfb
QEMU := qemu-system-i386
QEMU_COMMON_ARGS := -cdrom $(ISO) -no-reboot -no-shutdown
QEMU_GUI_ARGS := -display gtk,grab-on-hover=off,show-tabs=off
QEMU_SERIAL_LOG := $(BUILD_DIR)/qemu-serial.log

CC := gcc
LD := ld
NASM := nasm
PYTHON := python3

CFLAGS := -std=gnu11 -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -O2 -Wall -Wextra -Iinclude
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
ASFLAGS := -f elf32

USER_CFLAGS := -std=gnu11 -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -fomit-frame-pointer -O2 -Wall -Wextra -Iuser/lib -Iinclude
USER_LDFLAGS := -m elf_i386 -T user.ld -nostdlib

KERNEL_C_SOURCES := \
	src/kernel.c \
	src/console.c \
	src/serial.c \
	src/string.c \
	src/format.c \
	src/panic.c \
	src/gdt.c \
	src/pic.c \
	src/idt.c \
	src/pit.c \
	src/keyboard.c \
	src/power.c \
	src/memory.c \
	src/initrd.c \
	src/vfs.c \
	src/elf.c \
	src/task.c \
	src/syscall.c \
	src/shell.c

KERNEL_ASM_SOURCES := \
	src/boot.asm \
	src/isr.asm \
	src/task.asm

KERNEL_C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/src/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJECTS := $(BUILD_DIR)/src/boot.o $(BUILD_DIR)/src/isr.o $(BUILD_DIR)/src/task_asm.o
KERNEL_OBJECTS := $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS)

USER_LIB_SOURCES := \
	user/lib/start.c \
	user/lib/syscall.c \
	user/lib/runtime.c

USER_OBJECTS_hello := $(patsubst user/%.c,$(BUILD_DIR)/user/%.o,user/hello.c) $(patsubst user/lib/%.c,$(BUILD_DIR)/user/lib/%.o,$(USER_LIB_SOURCES))
USER_OBJECTS_uname := $(patsubst user/%.c,$(BUILD_DIR)/user/%.o,user/uname.c) $(patsubst user/lib/%.c,$(BUILD_DIR)/user/lib/%.o,$(USER_LIB_SOURCES))
USER_OBJECTS_toolbox := $(patsubst user/%.c,$(BUILD_DIR)/user/%.o,user/toolbox.c) $(patsubst user/lib/%.c,$(BUILD_DIR)/user/lib/%.o,$(USER_LIB_SOURCES))
USER_PROGRAMS := hello uname toolbox
USER_BINS := $(addprefix $(BUILD_DIR)/user/,$(addsuffix .elf,$(USER_PROGRAMS)))

.PHONY: all clean iso run run-gui run-debug ports ports-newlib ports-fastfetch ports-xorgproto ports-xtrans ports-zlib ports-libsha1 ports-libxau ports-libfontenc ports-x11 ports-pixman ports-libxkbfile ports-libxfont ports-xserver

all: $(ISO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/src:
	mkdir -p $(BUILD_DIR)/src

$(BUILD_DIR)/user:
	mkdir -p $(BUILD_DIR)/user

$(BUILD_DIR)/user/lib:
	mkdir -p $(BUILD_DIR)/user/lib

$(PORTS_DIR):
	mkdir -p $(PORTS_DIR)

$(BUILD_DIR)/src/%.o: src/%.c | $(BUILD_DIR)/src
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/%.o: src/%.asm | $(BUILD_DIR)/src
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/src/task_asm.o: src/task.asm | $(BUILD_DIR)/src
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/user/%.o: user/%.c | $(BUILD_DIR)/user
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/lib/%.o: user/lib/%.c | $(BUILD_DIR)/user/lib
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(KERNEL): $(KERNEL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(BUILD_DIR)/user/hello.elf: $(USER_OBJECTS_hello) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_hello)

$(BUILD_DIR)/user/uname.elf: $(USER_OBJECTS_uname) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_uname)

$(BUILD_DIR)/user/toolbox.elf: $(USER_OBJECTS_toolbox) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_toolbox)

$(PORTS_SYSROOT)/.newlib.stamp: ports/newlib/build-newlib.sh $(NEWLIB_PORT_FILES) | $(PORTS_DIR)
	ports/newlib/build-newlib.sh $(PORTS_DIR)/newlib $(PORTS_SYSROOT)
	touch $@

$(XORGPROTO_PC): ports/xorgproto/build-xorgproto.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PORTS_SYSROOT)/.newlib.stamp
	ports/xorgproto/build-xorgproto.sh $(PORTS_DIR)/xorgproto $(PORTS_SYSROOT)

$(XTRANS_PC): ports/xtrans/build-xtrans.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/xtrans/build-xtrans.sh $(PORTS_DIR)/xtrans $(PORTS_SYSROOT)

$(ZLIB_PC): ports/zlib/build-zlib.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/zlib/build-zlib.sh $(PORTS_DIR)/zlib $(PORTS_SYSROOT)

$(LIBSHA1_PC): ports/libsha1/build-libsha1.sh ports/libsha1/libsha1.c ports/libsha1/libsha1.h $(PORTS_SYSROOT)/.newlib.stamp
	ports/libsha1/build-libsha1.sh $(PORTS_DIR)/libsha1 $(PORTS_SYSROOT)

$(XAU_PC): ports/libxau/build-libxau.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(XORGPROTO_PC) $(PORTS_SYSROOT)/.newlib.stamp
	ports/libxau/build-libxau.sh $(PORTS_DIR)/libxau $(PORTS_SYSROOT)

$(FONTENC_PC): ports/libfontenc/build-libfontenc.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(XORGPROTO_PC) $(ZLIB_PC) $(PORTS_SYSROOT)/.newlib.stamp
	ports/libfontenc/build-libfontenc.sh $(PORTS_DIR)/libfontenc $(PORTS_SYSROOT)

$(X11_PC): ports/x11/build-x11.sh ports/x11/compat_xlib.c $(XORGPROTO_PC) $(PORTS_SYSROOT)/.newlib.stamp
	ports/x11/build-x11.sh $(PORTS_DIR)/x11 $(PORTS_SYSROOT)

$(PIXMAN_PC): ports/pixman/build-pixman.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PORTS_SYSROOT)/.newlib.stamp
	ports/pixman/build-pixman.sh $(PORTS_DIR)/pixman $(PORTS_SYSROOT)

$(XKBFILE_PC): ports/libxkbfile/build-libxkbfile.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(XORGPROTO_PC) $(X11_PC) $(PORTS_SYSROOT)/.newlib.stamp
	ports/libxkbfile/build-libxkbfile.sh $(PORTS_DIR)/libxkbfile $(PORTS_SYSROOT)

$(XFONT2_PC): ports/libxfont/build-libxfont.sh $(XORGPROTO_PC) $(XTRANS_PC) $(FONTENC_PC) $(ZLIB_PC) $(PORTS_SYSROOT)/.newlib.stamp
	ports/libxfont/build-libxfont.sh $(PORTS_DIR)/libxfont $(PORTS_SYSROOT)

$(FASTFETCH_BIN): ports/fastfetch/build-fastfetch.sh ports/fastfetch/openhobbyos-toolchain.cmake $(PORTS_SYSROOT)/.newlib.stamp
	ports/fastfetch/build-fastfetch.sh $(PORTS_DIR)/fastfetch $(PORTS_SYSROOT)

$(XSERVER_BIN): ports/xserver/build-xserver.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(XSERVER_DEPS)
	ports/xserver/build-xserver.sh $(PORTS_DIR)/xserver $(PORTS_SYSROOT)

$(INITRD): tools/build_initrd.sh tools/mkramdisk.py assets/etc/motd.txt assets/etc/os-release assets/etc/xdg/fastfetch/config.jsonc assets/etc/xdg/fastfetch/minimal.jsonc $(USER_BINS) $(FASTFETCH_BIN) $(XSERVER_BIN) | $(BUILD_DIR)
	tools/build_initrd.sh $@

$(ISO): $(KERNEL) $(INITRD) grub/grub.cfg | $(BUILD_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	cp $(INITRD) $(ISO_DIR)/boot/initrd.bin
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR)

iso: $(ISO)

ports-newlib: $(PORTS_SYSROOT)/.newlib.stamp

ports-fastfetch: $(FASTFETCH_BIN)

ports-xorgproto: $(XORGPROTO_PC)

ports-xtrans: $(XTRANS_PC)

ports-zlib: $(ZLIB_PC)

ports-libsha1: $(LIBSHA1_PC)

ports-libxau: $(XAU_PC)

ports-libfontenc: $(FONTENC_PC)

ports-x11: $(X11_PC)

ports-pixman: $(PIXMAN_PC)

ports-libxkbfile: $(XKBFILE_PC)

ports-libxfont: $(XFONT2_PC)

ports-xserver: $(XSERVER_BIN)

ports: $(FASTFETCH_BIN) $(XSERVER_BIN)

run: run-gui

run-gui: $(ISO)
	mkdir -p $(BUILD_DIR)
	$(QEMU) $(QEMU_COMMON_ARGS) $(QEMU_GUI_ARGS) -serial file:$(QEMU_SERIAL_LOG)

run-debug: $(ISO)
	$(QEMU) $(QEMU_COMMON_ARGS) $(QEMU_GUI_ARGS) -serial stdio

clean:
	rm -rf $(BUILD_DIR)
