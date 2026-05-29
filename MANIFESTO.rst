===============================================================================
                         OpenHobbyOS (OHOS) Manifesto
===============================================================================

.. contents:: Table of Contents
   :depth: 3
   :local:

Overview
========

OpenHobbyOS is a 32-bit x86 operating system written in C with NASM assembly.
It features a custom kernel with Linux ABI compatibility, a ported NewLib C
library, libtsm-based framebuffer console, lwIP TCP/IP stack, uACPI power
management, and XNX — a display protocol using Unix domain sockets and pixman
software compositing. The OS is designed as a learning project with a focus on
staying small while supporting modern features.

**Target Architecture:** i386 (32-bit x86)
**Boot Method:** Multiboot-compliant bootloader (GRUB)
**License:** BSD 3-Clause
**Memory Model:** Higher-half kernel (3GB) with per-process page directories

===============================================================================
                           REPOSITORY STRUCTURE
===============================================================================

Root Files
----------

.. list-table:: Root Directory Files
   :header-rows: 1

   * - File
     - Purpose
   * - LICENSE
     - BSD 3-Clause license
   * - Makefile
     - Main build system (kernel, userland, ports, ISO)
   * - README.md
     - Project overview and quick start
   * - MANIFESTO.rst
     - This file — comprehensive architecture documentation
   * - linker.ld
     - Kernel linker script (loads at 1MB)
   * - user.ld
     - Userland program linker script (loads at 0x03000000)
   * - .gitignore
     - Git ignore patterns
   * - .gitmodules
     - Git submodule definitions

Source Code (src/)
==================

The kernel is written in C with x86 NASM assembly stubs. It provides a
Linux-compatible syscall ABI, preemptive multitasking, VFS, memory management,
device drivers, networking, and power management.

Kernel Entry & Core
-------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - boot.asm
     - Multiboot-compliant entry point, stack setup, framebuffer init
   * - kernel.c
     - kernel_main(): full device initialization orchestration
   * - panic.c
     - Kernel panic handler
   * - compiler_rt.c
     - Compiler runtime helpers (64-bit division, etc.)

CPU / Architecture
------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - gdt.c
     - Global Descriptor Table setup (segments, TSS for user mode)
   * - idt.c
     - Interrupt Descriptor Table, exception handlers
   * - pic.c
     - 8259 Programmable Interrupt Controller
   * - pit.c
     - 8254 Programmable Interval Timer (100Hz scheduler tick)
   * - isr.asm
     - Low-level interrupt stub routines
   * - task.asm
     - Context switch assembly (save/restore registers, CR3 switch)

Memory Management
-----------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - memory.c
     - Kernel heap allocator (kmalloc/kfree/krealloc), defragmentation
   * - paging.c
     - x86 paging subsystem:
       * Physical frame allocator (bitmap-based)
       * Page directory/table management
       * Per-process address spaces
       * Copy-on-write support
       * Page fault handler
       * Higher-half kernel mappings

Device Drivers
--------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - console.c
     - VGA text mode + libtsm framebuffer console with VT100 emulation
   * - serial.c
     - COM1 serial port driver (debug output)
   * - keyboard.c
     - PS/2 keyboard driver with scancode translation
   * - ata.c
     - ATA/IDE disk driver (PIO mode)
   * - pci.c
     - PCI bus scanner
   * - rtl8139.c
     - RTL8139 Fast Ethernet NIC driver
   * - netdev.c
     - Network device abstraction layer

Storage & Filesystems
---------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - blkdev.c
     - Block device abstraction layer
   * - mbr.c
     - Master Boot Record parser
   * - ext2.c
     - EXT2 filesystem driver (read/write)
   * - initrd.c
     - Initial RAM disk loader (multiboot module)
   * - vfs.c
     - Virtual Filesystem Switch — initrd/ext2/devfs merge, overlay support

Process/Task Management
-----------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - task.c
     - Scheduler (round-robin), process creation, ELF loading, COW fork,
       FPU init, context switching with paging integration
   * - elf.c
     - ELF32 executable loader with demand paging infrastructure

System Calls & IPC
------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - syscall.c
     - Linux-compatible syscall dispatch table (80+ syscalls via int 0x80)
   * - socket.c
     - Unix domain sockets (AF_UNIX, SOCK_STREAM) for local IPC
   * - pipe.c
     - Anonymous pipe implementation

Power Management
----------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - power.c
     - uACPI-backed ACPI parser, shutdown/reboot/suspend
   * - uacpi_port.c
     - uACPI kernel API port layer (mapping, PCI, timer, logging)

Compatibility
-------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - tsm_compat.c
     - libtsm kernel compatibility (malloc/free/realloc wrappers)

Utilities
---------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - string.c
     - Kernel string functions (memcpy, memset, strlen, strcmp, etc.)
   * - format.c
     - printf-style formatting (snprintf, vsnprintf)
   * - shell.c
     - Built-in kernel shell with program autodiscovery and fastfetch integration

Headers (include/)
==================

Public headers used by both kernel and userland.

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - types.h
     - Fixed-width types (u8, u16, u32, u64, i32, etc.)
   * - io.h
     - Port I/O macros (inb, outb, etc.), CR0/CR4 helpers
   * - multiboot.h
     - Multiboot specification structures (framebuffer, modules, memory map)
   * - abi/linux.h
     - Linux ABI constants (syscall numbers, errno, signal, socket structs)
   * - assert.h
     - Assert macro
   * - errno.h
     - Error number definitions
   * - limits.h
     - Integer limits
   * - inttypes.h
     - printf format macros for fixed-width types
   * - stdio.h
     - Standard I/O declarations (snprintf, vsnprintf)
   * - stdlib.h
     - Standard library (malloc/free/abort)
   * - string.h
     - String function prototypes
   * - signal.h
     - Signal handling definitions
   * - thread.h
     - Thread-related declarations
   * - unistd.h
     - POSIX-like system call constants

.. list-table::
   :header-rows: 1

   * - File
     - Description (Kernel-only headers)
   * - ata.h
     - ATA driver interface
   * - blkdev.h
     - Block device structures
   * - console.h
     - Console output API, framebuffer info, terminal response
   * - console_font.h
     - Built-in framebuffer font data
   * - elf.h
     - ELF32 format definitions
   * - ext2.h
     - EXT2 filesystem structures
   * - gdt.h
     - GDT/TSS setup interface
   * - idt.h
     - IDT/interrupt interface
   * - initrd.h
     - Initrd loader interface
   * - keyboard.h
     - Keyboard scancode constants
   * - mbr.h
     - Master Boot Record structures
   * - memory.h
     - Memory allocator interface, page alignment, heap stats
   * - netdev.h
     - Network device abstraction
   * - panic.h
     - Panic macro
   * - paging.h
     - Paging subsystem interface (page tables, frame allocator, TLB ops)
   * - pagecache.h
     - Page cache structures
   * - pci.h
     - PCI bus interface
   * - pic.h
     - 8259 PIC interface
   * - pipe.h
     - Pipe structures and API
   * - pit.h
     - PIT timer interface
   * - power.h
     - Power management interface
   * - rtl8139.h
     - RTL8139 NIC driver interface
   * - serial.h
     - Serial driver interface
   * - shell.h
     - Shell entry point
   * - socket.h
     - Socket structures and API
   * - syscall.h
     - Syscall number definitions
   * - task.h
     - Task/process structures, scheduler API, FPU init
   * - vfs.h
     - VFS node structures and operations

Userspace (user/)
=================

Native OHOS programs using the custom syscall ABI (int 0x80).

Programs
--------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - hello.c
     - Test program demonstrating userland execution
   * - uname.c
     - Print system information
   * - toolbox.c
     - Multi-call binary (ls, cat, cp, mv, rm, mkdir, echo, sleep, clear, etc.)
   * - sh.c
     - Simple POSIX-like shell
   * - gosh.c
     - GOSH shell v2 — full-featured interactive shell (default /bin/sh)
   * - test_fb.c
     - Framebuffer test (draws patterns on screen)
   * - net_test.c
     - Network diagnostics (MAC address, device info)
   * - net_info.c
     - Network information display

XNX Display Protocol (user/xnx/)
---------------------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - compositor.c
     - XNX compositor: framebuffer, pixman compositing, socket listener
   * - demo.c
     - XNX demo client: connects to compositor, draws animated gradient
   * - protocol.h
     - XNX wire protocol definitions
   * - strtod_stub.c
     - strtod stub for the compositor (no FPU)

Userspace Library (user/lib/)
-----------------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - start.c
     - _start() entry point, argc/argv/envp setup, init/fini array support
   * - syscall.c
     - Syscall wrapper implementations (int 0x80 ABI)
   * - syscall.h
     - Syscall function prototypes
   * - runtime.c
     - C runtime (formatted print, malloc/free via sbrk, string utils)
   * - runtime.h
     - Runtime library headers
   * - newlib-gloss.c
     - NewLib OS syscall glue layer (low-level OS bindings)
   * - eth.c / eth.h
     - Ethernet helper utilities
   * - xnx/xnx.c
     - XNX client library (connect, surfaces, buffers, commit)
   * - xnx/xnx.h
     - XNX client library header

Third-Party Libraries (user/lib/)
---------------------------------

These are Git submodules providing library code for ports and kernel:

.. list-table::
   :header-rows: 1

   * - Directory
     - Description
   * - fastfetch/
     - System info display tool
   * - newlib-cygwin/
     - NewLib C library (cross-compiled for userspace)
   * - pixman/
     - Software pixel compositing library
   * - zlib/
     - Compression library
   * - libtsm/
     - Terminal emulator library (linked into kernel)
   * - lwip/
     - lwIP TCP/IP stack
   * - lodepng/
     - PNG encoder/decoder
   * - doomgeneric/
     - Doom game engine port layer
   * - uACPI/
     - ACPI implementation (linked into kernel)

Build System (tools/)
=====================

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - build_initrd.sh
     - Create initial ramdisk from rootfs manifest
   * - mkramdisk.py
     - Python ramdisk generator (OHOSRD1 format)
   * - rootfs_manifest.sh
     - Define files to include in rootfs
   * - create_disk.sh
     - Create blank MBR disk image
   * - populate_disk.sh
     - Populate ext2 disk image with rootfs
   * - ensure_meson.sh
     - Setup Python virtualenv with Meson
   * - meson-requirements.txt
     - Python dependencies for Meson

Cross-Compiler (toolchain/)
=============================

.. list-table::
   :header-rows: 1

   * - Path
     - Description
   * - bin/i686-openhobbyos-*
     - GCC toolchain wrappers for OHOS target (gcc, as, ld, ar, nm, etc.)
   * - bin/i686-elf-*
     - Standard ELF32 toolchain wrappers

Ports (ports/)
==============

Third-party software ports to OHOS, built via shell scripts.

C Library
---------

.. list-table::
   :header-rows: 1

   * - Directory
     - Description
   * - newlib/
     - NewLib C library port with OHOS-specific compat layers

Port Build Scripts
------------------

.. list-table::
   :header-rows: 1

   * - Script
     - Builds
   * - newlib/build-newlib.sh
     - NewLib C library (autotools)
   * - zlib/build-zlib.sh
     - zlib compression library
   * - libsha1/build-libsha1.sh
     - SHA1 implementation
   * - pixman/build-pixman.sh
     - Pixman (meson cross-compile)
   * - lodepng/build-lodepng.sh
     - LodePNG encoder/decoder
   * - lwip/build-lwip.sh
     - lwIP TCP/IP stack
   * - fastfetch/build-fastfetch.sh
     - Fastfetch system info (cmake)
   * - xnx/build-xnx.sh
     - XNX compositor + client library + demo
   * - doom/build-doom.sh
     - Doom game via doomgeneric

Resources (assets/)
===================

.. list-table::
   :header-rows: 1

   * - Path
     - Description
   * - etc/motd.txt
     - Message of the day (displayed on boot)
   * - etc/goshrc
     - GOSH shell startup configuration
   * - etc/os-release
     - OS identification (NAME, VERSION, etc.)
   * - etc/xdg/fastfetch/config.jsonc
     - Fastfetch configuration with OHOS logo
   * - etc/xdg/fastfetch/minimal.jsonc
     - Minimal fastfetch preset
   * - etc/xdg/fastfetch/ohos_logo.txt
     - Custom OHOS ASCII logo (hexagon)
   * - etc/xdg/fastfetch/ohos_logo_small.txt
     - Small OHOS ASCII logo
   * - shell-colors.jsonc
     - Terminal color theme
   * - Doom1.WAD
     - Doom game data

Boot (grub/)
============

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - grub.cfg
     - GRUB2 boot menu configuration (multiboot, framebuffer)

===============================================================================
                              SYSTEM ARCHITECTURE
===============================================================================

1. Boot Process
===============

::

   GRUB (on ISO) → boot.asm → kernel_main()

1. GRUB loads kernel.bin at 1MB (per linker.ld)
2. boot.asm sets up stack, zeroes BSS, calls kernel_main()
3. kernel_main() initializes subsystems:

   - console (early VGA output)
   - GDT (segmentation + TSS for user mode)
   - PIC (interrupt controller)
   - IDT (interrupt handlers)
   - PIT (100Hz timer for scheduling)
   - Keyboard (PS/2)
   - Memory manager (kernel heap + frame allocator)
   - Paging (page tables, frame allocator — paging not enabled, virtual == physical)
   - Framebuffer console (libtsm activation)
   - Initrd (multiboot module → filesystem)
   - VFS (mounts initrd at /)
   - PCI scan, network init
   - Task scheduler
   - FPU init
   - Power management (uACPI)
   - Shell or userspace init

2. Memory Layout
================

::

   0x00000000 - 0x0009FFFF: Low memory (reserved)
   0x000A0000 - 0x000FFFFF: VGA/ROM (reserved)
   0x00100000 - kernel_end:   Kernel code/data (identity mapped)
   kernel_end - 0x02FF0000:   Kernel heap (grows upward)
   0x03000000+:               User ELF loading (48MB+)

.. note::
   Paging subsystem is fully initialized (page tables, frame allocator, per-process
   page directories) but **paging is not enabled on the CPU**. Virtual addresses
   equal physical addresses. USER_BASE at 0x03000000 was chosen to sit above
   the kernel heap (~0x02FF0000).

3. Process Model
================

Preemptive Multitasking
-----------------------

.. list-table::
   :header-rows: 1

   * - Component
     - Implementation
   * - Scheduler
     - Round-robin, 100Hz PIT timer
   * - Context Switch
     - CR3 register switch (prepared, CPU ignores without paging)
   * - Address Space
     - Per-process page directory
   * - Kernel Entry
     - Syscalls via interrupt 0x80
   * - Kernel Exit
     - iretd to user space
   * - FPU Support
     - fpu_init(), save/restore on context switch

Process Slot Limit
------------------

Maximum 8 concurrent user processes (fixed-size slot array).

4. Syscall Interface
====================

Linux ABI-compatible syscall numbers (from abi/linux.h), dispatched via int 0x80.

**Key syscall groups:**

* File: open, read, write, close, lseek, stat, mkdir, unlink, fcntl, ioctl
* Process: fork, execve, exit, waitpid, getpid, getppid
* Memory: brk, mmap2, munmap, mprotect
* IPC: pipe2, socket, bind, connect, send, recv, listen, accept
* Time: clock_gettime, nanosleep, gettimeofday
* Signals: kill, sigaction, sigreturn, sigprocmask
* OHOS-specific: spawn, yield, getdents (400+ range)

5. Filesystem Stack
===================

::

   VFS → initrd (cpio, read-only) | ext2 (read/write) | devfs (/dev/null, /dev/zero, /dev/net)

Block layer: VFS → blkdev → ATA driver → Disk

The initrd is loaded as a multiboot module and serves as the root filesystem.
An ext2 disk image can be attached for persistent storage; files present on the
disk overlay the initrd contents.

6. Graphics Stack
=================

Console
-------

VGA text mode (80x25, early boot) → libtsm framebuffer (GUI mode, activated after
framebuffer detection) → Serial port (debug fallback).

The console uses libtsm for VT100 terminal emulation with a built-in 8x16 bitmap
font. It supports ANSI colors, cursor positioning, scrollback buffer (5000 lines),
and terminal response queries.

XNX Display Protocol
--------------------

* Minimal display protocol using Unix domain sockets (AF_UNIX, SOCK_STREAM)
* Compositor runs as background userspace process (/bin/xnx-compositor)
* Clients connect to /tmp/xnx.sock, create surfaces, write pixel buffers
* Uses pixman for software compositing (PIXMAN_OP_OVER)
* Wire protocol: fixed-header + payload messages
* Demo client (/bin/xnx-demo) draws animated gradients as a test

7. Networking Stack
===================

::

   Userspace → syscall → netdev → rtl8139 driver → PCI → Hardware

The RTL8139 NIC driver handles packet RX/TX via PIO. A network device
abstraction layer (netdev) allows userspace programs to open /dev/net and
send/receive raw Ethernet frames. The lwIP TCP/IP stack can be cross-compiled
as a port for higher-level protocol support.

===============================================================================
                             PAGING SUBSYSTEM
===============================================================================

Overview
--------

.. note::
   The paging subsystem is fully wired — page tables exist, the frame allocator
   runs, per-process page directories are created/destroyed, CR3 is set during
   context switches — but ``paging_enable()`` is never called. The CPU runs
   with paging disabled, so virtual = physical. This means user ELF loading at
   USER_BASE writes to physical RAM at that address, which is why USER_BASE
   (0x03000000) was chosen to sit above the kernel heap (~0x02FF0000).

* Physical frame allocator (bitmap-based, from kernel_end+)
* Per-process page directories with kernel space shared (768-1023)
* Page mapping/unmapping operations
* Copy-on-write page table cloning
* Page fault handler (ready but unused)
* Higher-half kernel mappings (3GB+ shared across processes)

===============================================================================
                            FILE COUNT SUMMARY
===============================================================================

Source Code
-----------

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - Kernel (src/)
     - 35 files (32 C + 3 NASM)
   * - Headers (include/)
     - 39 files
   * - Userspace (user/)
     - 8 native programs + XNX (4 files) + library (11 files)

Build System
------------

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - Makefile
     - 1 file
   * - Linker scripts
     - 2 files
   * - Build scripts (tools/)
     - 7 files

Ports
-----

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - Port build scripts
     - 9 scripts
   * - NewLib compat layer
     - 16 files

Configuration
-------------

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - GRUB config
     - 1 file
   * - Assets (configs, artwork)
     - 10 files

===============================================================================
                              BUILD PIPELINE
===============================================================================

1. **Kernel:** C/ASM sources → GCC/NASM → LD (linker.ld) → ``kernel.bin``
2. **libtsm:** Library sources → GCC → object files linked into kernel
3. **uACPI:** Library sources → GCC → object files linked into kernel
4. **Userlib:** ``user/lib/*.c`` → GCC → object files
5. **User programs:** ``user/*.c`` → link with userlib objects (user.ld) → ``.elf``
6. **Ports:** Build scripts → cross-compile → sysroot (``build/ports/sysroot/``)
7. **Initrd:** User .elf files + assets → ``mkramdisk.py`` → ``initrd.bin``
8. **ISO:** ``kernel.bin`` + ``initrd.bin`` + GRUB config → ``grub-mkrescue``

===============================================================================
                            RUN TARGETS
===============================================================================

.. list-table::
   :header-rows: 1

   * - Command
     - Description
   * - ``make`` / ``make all``
     - Build the ISO
   * - ``make run`` / ``make run-gui``
     - Run in QEMU (GTK display, serial log)
   * - ``make run-debug``
     - Run with serial output to terminal
   * - ``make run-with-disk`` / ``make run-disk``
     - Run with attached disk image
   * - ``make clean``
     - Remove build artifacts
   * - ``make ports``
     - Build all ports (fastfetch + XNX)
   * - ``make ports-newlib``
     - Build newlib only

===============================================================================
                               END OF MANIFESTO
===============================================================================
