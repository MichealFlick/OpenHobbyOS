PROJECT := OpenHobbyOS
BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.bin
INITRD := $(BUILD_DIR)/initrd.bin
ISO := $(BUILD_DIR)/$(PROJECT).iso
PORTS_DIR := $(BUILD_DIR)/ports
PORTS_SYSROOT := $(PORTS_DIR)/sysroot
NEWLIB_PORT_FILES := $(shell find ports/newlib/openhobbyos -type f | sort)
ZLIB_PC := $(PORTS_SYSROOT)/lib/pkgconfig/zlib.pc
LIBSHA1_PC := $(PORTS_SYSROOT)/lib/pkgconfig/libsha1.pc
PIXMAN_PC := $(PORTS_SYSROOT)/lib/pkgconfig/pixman-1.pc
FASTFETCH_BIN := $(PORTS_DIR)/fastfetch/install/usr/bin/fastfetch
XNX_COMPOSITOR := $(PORTS_DIR)/xnx/install/bin/xnx-compositor
PORTS_LUA_BIN := $(PORTS_SYSROOT)/bin/lua
PORTS_TINYGL_A := $(PORTS_SYSROOT)/lib/libtinygl.a
PORTS_GEARS_BIN := $(PORTS_SYSROOT)/bin/gears
QEMU := qemu-system-i386
DISK_IMG := disk.img
QEMU_COMMON_ARGS := -cdrom $(ISO) -no-reboot -no-shutdown
QEMU_DISK_ARGS := -drive file=$(DISK_IMG),format=raw,index=0,media=disk -boot d
QEMU_GUI_ARGS := -display gtk,grab-on-hover=off,show-tabs=off
QEMU_SERIAL_LOG := $(BUILD_DIR)/qemu-serial.log
QEMU_RUN_CD := $(QEMU) $(QEMU_COMMON_ARGS) $(QEMU_GUI_ARGS)
QEMU_RUN_DISK := $(QEMU) $(QEMU_COMMON_ARGS) $(QEMU_DISK_ARGS) $(QEMU_GUI_ARGS)

CC := gcc
LD := ld
NASM := nasm
PYTHON := python3

CFLAGS := -std=gnu11 -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -O2 -Wall -Wextra -D__OHOS_KERNEL__ -Iinclude -Iuser/lib/libtsm/src/tsm -Iuser/lib/libtsm/src/shared -Iuser/lib/libtsm/external/wcwidth -Iuser/lib/uACPI/include
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
	src/paging.c \
	src/blkdev.c \
	src/ata.c \
	src/mbr.c \
	src/ext2.c \
	src/initrd.c \
	src/vfs.c \
	src/elf.c \
	src/task.c \
	src/syscall.c \
	src/socket.c \
	src/pipe.c \
	src/shell.c \
	src/compiler_rt.c \
	src/tsm_compat.c \
	src/uacpi_port.c \
	src/netdev.c \
	src/pci.c \
	src/rtl8139.c

KERNEL_ASM_SOURCES := \
	src/boot.asm \
	src/isr.asm \
	src/task.asm

LIBTSM_C_SOURCES := \
	user/lib/libtsm/src/tsm/tsm-screen.c \
	user/lib/libtsm/src/tsm/tsm-vte.c \
	user/lib/libtsm/src/tsm/tsm-unicode.c \
	user/lib/libtsm/src/tsm/tsm-render.c \
	user/lib/libtsm/src/tsm/tsm-selection.c \
	user/lib/libtsm/src/tsm/tsm-vte-charsets.c \
	user/lib/libtsm/src/shared/shl-htable.c \
	user/lib/libtsm/external/wcwidth/wcwidth.c

UACPI_C_SOURCES := \
	user/lib/uACPI/source/tables.c \
	user/lib/uACPI/source/types.c \
	user/lib/uACPI/source/uacpi.c \
	user/lib/uACPI/source/utilities.c \
	user/lib/uACPI/source/interpreter.c \
	user/lib/uACPI/source/opcodes.c \
	user/lib/uACPI/source/namespace.c \
	user/lib/uACPI/source/stdlib.c \
	user/lib/uACPI/source/shareable.c \
	user/lib/uACPI/source/opregion.c \
	user/lib/uACPI/source/default_handlers.c \
	user/lib/uACPI/source/io.c \
	user/lib/uACPI/source/notify.c \
	user/lib/uACPI/source/sleep.c \
	user/lib/uACPI/source/registers.c \
	user/lib/uACPI/source/resources.c \
	user/lib/uACPI/source/event.c \
	user/lib/uACPI/source/mutex.c \
	user/lib/uACPI/source/osi.c

KERNEL_C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/src/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJECTS := $(BUILD_DIR)/src/boot.o $(BUILD_DIR)/src/isr.o $(BUILD_DIR)/src/task_asm.o
LIBTSM_OBJECTS := \
	$(BUILD_DIR)/libtsm/tsm/tsm-screen.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-vte.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-unicode.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-render.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-selection.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-vte-charsets.o \
	$(BUILD_DIR)/libtsm/shared/shl-htable.o \
	$(BUILD_DIR)/libtsm/external/wcwidth/wcwidth.o
UACPI_OBJECTS := $(patsubst user/lib/uACPI/source/%.c,$(BUILD_DIR)/uacpi/%.o,$(UACPI_C_SOURCES))
KERNEL_OBJECTS := $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(LIBTSM_OBJECTS) $(UACPI_OBJECTS)

USER_LIB_SOURCES := \
	user/lib/start.c \
	user/lib/syscall.c \
	user/lib/newlib-gloss.c \
	user/lib/runtime.c

USER_PROGRAMS := hello uname toolbox sh gosh test_fb net_test net_info

define user_program_template
USER_OBJECTS_$(1) := $(patsubst user/%.c,$(BUILD_DIR)/user/%.o,user/$(1).c) $$(patsubst user/lib/%.c,$(BUILD_DIR)/user/lib/%.o,$(USER_LIB_SOURCES))
endef
$(foreach prog,$(USER_PROGRAMS),$(eval $(call user_program_template,$(prog))))
USER_BINS := $(addprefix $(BUILD_DIR)/user/,$(addsuffix .elf,$(USER_PROGRAMS)))

.PHONY: all clean iso disk disk-img run run-gui run-debug run-with-disk run-disk ports ports-newlib ports-fastfetch ports-zlib ports-libsha1 ports-pixman ports-xnx ports-lodepng ports-lwip ports-doom ports-lua

all: $(ISO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/src:
	mkdir -p $(BUILD_DIR)/src

$(BUILD_DIR)/user:
	mkdir -p $(BUILD_DIR)/user

$(BUILD_DIR)/user/lib:
	mkdir -p $(BUILD_DIR)/user/lib

$(BUILD_DIR)/libtsm/tsm:
	mkdir -p $(BUILD_DIR)/libtsm/tsm

$(BUILD_DIR)/libtsm/shared:
	mkdir -p $(BUILD_DIR)/libtsm/shared

$(BUILD_DIR)/libtsm/external/wcwidth:
	mkdir -p $(BUILD_DIR)/libtsm/external/wcwidth

$(BUILD_DIR)/uacpi:
	mkdir -p $(BUILD_DIR)/uacpi

$(PORTS_DIR):
	mkdir -p $(PORTS_DIR)

$(BUILD_DIR)/src/%.o: src/%.c | $(BUILD_DIR)/src
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/boot.o: src/boot.asm | $(BUILD_DIR)/src
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/src/isr.o: src/isr.asm | $(BUILD_DIR)/src
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/src/task_asm.o: src/task.asm | $(BUILD_DIR)/src
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/libtsm/tsm/%.o: user/lib/libtsm/src/tsm/%.c | $(BUILD_DIR)/libtsm/tsm
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libtsm/shared/%.o: user/lib/libtsm/src/shared/%.c | $(BUILD_DIR)/libtsm/shared
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libtsm/external/wcwidth/%.o: user/lib/libtsm/external/wcwidth/%.c | $(BUILD_DIR)/libtsm/external/wcwidth
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uacpi/%.o: user/lib/uACPI/source/%.c | $(BUILD_DIR)/uacpi
	$(CC) $(CFLAGS) -c $< -o $@

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

$(BUILD_DIR)/user/sh.elf: $(USER_OBJECTS_sh) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_sh)

$(BUILD_DIR)/user/gosh.elf: $(USER_OBJECTS_gosh) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_gosh)

$(BUILD_DIR)/user/test_fb.elf: $(USER_OBJECTS_test_fb) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_test_fb)

$(BUILD_DIR)/user/net_test.elf: $(USER_OBJECTS_net_test) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_net_test)

$(BUILD_DIR)/user/net_info.elf: $(USER_OBJECTS_net_info) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_net_info)

$(PORTS_SYSROOT)/.newlib.stamp: ports/newlib/build-newlib.sh $(NEWLIB_PORT_FILES) | $(PORTS_DIR)
	ports/newlib/build-newlib.sh $(PORTS_DIR)/newlib $(PORTS_SYSROOT)
	touch $@

$(ZLIB_PC): ports/zlib/build-zlib.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/zlib/build-zlib.sh $(PORTS_DIR)/zlib $(PORTS_SYSROOT)

$(LIBSHA1_PC): ports/libsha1/build-libsha1.sh ports/libsha1/libsha1.c ports/libsha1/libsha1.h $(PORTS_SYSROOT)/.newlib.stamp
	ports/libsha1/build-libsha1.sh $(PORTS_DIR)/libsha1 $(PORTS_SYSROOT)

$(PIXMAN_PC): ports/pixman/build-pixman.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PORTS_SYSROOT)/.newlib.stamp
	ports/pixman/build-pixman.sh $(PORTS_DIR)/pixman $(PORTS_SYSROOT)

$(FASTFETCH_BIN): ports/fastfetch/build-fastfetch.sh ports/fastfetch/openhobbyos-toolchain.cmake $(PORTS_SYSROOT)/.newlib.stamp
	ports/fastfetch/build-fastfetch.sh $(PORTS_DIR)/fastfetch $(PORTS_SYSROOT)

$(XNX_COMPOSITOR): ports/xnx/build-xnx.sh $(PIXMAN_PC) $(PORTS_SYSROOT)/.newlib.stamp | $(PORTS_DIR)
	ports/xnx/build-xnx.sh $(PORTS_DIR)/xnx $(PORTS_SYSROOT)

$(INITRD): tools/build_initrd.sh tools/mkramdisk.py tools/rootfs_manifest.sh $(USER_BINS) $(FASTFETCH_BIN) $(XNX_COMPOSITOR) $(PORTS_LUA_BIN) $(PORTS_GEARS_BIN) | $(BUILD_DIR)
	tools/build_initrd.sh $@

$(DISK_IMG): $(USER_BINS) $(FASTFETCH_BIN) $(XNX_COMPOSITOR) $(PORTS_LUA_BIN) $(PORTS_GEARS_BIN) tools/populate_disk.sh tools/rootfs_manifest.sh
	sudo env OPENHOBBYOS_ROOT="$(CURDIR)" "$(CURDIR)/tools/populate_disk.sh" "$(CURDIR)/$(DISK_IMG)"

$(ISO): $(KERNEL) $(INITRD) grub/grub.cfg | $(BUILD_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	cp $(INITRD) $(ISO_DIR)/boot/initrd.bin
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR)

iso: $(ISO)

ports-newlib: $(PORTS_SYSROOT)/.newlib.stamp

ports-fastfetch: $(FASTFETCH_BIN)

ports-zlib: $(ZLIB_PC)

ports-libsha1: $(LIBSHA1_PC)

ports-pixman: $(PIXMAN_PC)

ports-xnx: $(XNX_COMPOSITOR)

ports-lodepng: ports/lodepng/build-lodepng.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/lodepng/build-lodepng.sh $(PORTS_DIR)/lodepng $(PORTS_SYSROOT)

ports-lwip: ports/lwip/build-lwip.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/lwip/build-lwip.sh $(PORTS_DIR)/lwip $(PORTS_SYSROOT)

ports-doom: ports/doom/build-doom.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/doom/build-doom.sh $(PORTS_DIR)/doom $(PORTS_SYSROOT)

ports-ffmpeg: ports/ffmpeg/build-ffmpeg.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/ffmpeg/build-ffmpeg.sh $(PORTS_DIR)/ffmpeg $(PORTS_SYSROOT)

$(PORTS_LUA_BIN): ports/lua/build-lua.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/lua/build-lua.sh $(PORTS_DIR)/lua $(PORTS_SYSROOT)

ports-lua: $(PORTS_LUA_BIN)

$(PORTS_TINYGL_A): ports/tinygl/build-tinygl.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/tinygl/build-tinygl.sh $(PORTS_DIR)/tinygl $(PORTS_SYSROOT)

ports-tinygl: $(PORTS_TINYGL_A)

$(PORTS_GEARS_BIN): ports/gears/build-gears.sh ports/gears/gears.c $(PORTS_TINYGL_A)
	ports/gears/build-gears.sh $(PORTS_DIR)/gears $(PORTS_SYSROOT)

ports-gears: $(PORTS_GEARS_BIN)

ports: $(FASTFETCH_BIN) $(XNX_COMPOSITOR) $(PORTS_LUA_BIN) $(PORTS_GEARS_BIN)

run: run-gui

run-gui: $(ISO)
	mkdir -p $(BUILD_DIR)
	$(QEMU_RUN_CD) -serial file:$(QEMU_SERIAL_LOG)

run-debug: $(ISO)
	$(QEMU_RUN_CD) -serial stdio

run-with-disk: $(ISO) $(DISK_IMG)
	mkdir -p $(BUILD_DIR)
	$(QEMU_RUN_DISK) -serial file:$(QEMU_SERIAL_LOG)

run-disk: run-with-disk

disk disk-img: $(DISK_IMG)

clean:
	rm -rf $(BUILD_DIR)
