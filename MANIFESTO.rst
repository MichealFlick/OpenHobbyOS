================================================================================
                        OpenHobbyOS (OHOS) Manifesto
================================================================================

this manifasto was written by Chris Uarkenchov

.. contents:: Table of Contents
   :depth: 3
   :local:

Overview
========

OpenHobbyOS is a 32-bit x86 compatibility-focused operating system written in C.
It features a custom kernel with Linux ABI compatibility, a ported NewLib C
library, and XNX — a purpose-built display protocol using unix domain sockets
and pixman software compositing. The OS is designed as a learning project with
a focus on staying small while supporting modern features.

**Target Architecture:** i386 (32-bit x86)  
**Boot Method:** Multiboot-compliant bootloader (GRUB)  
**License:** BSD 3-Clause  
**Memory Model:** Higher-half kernel (3GB) with per-process page directories

================================================================================
                           REPOSITORY STRUCTURE
================================================================================

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
     - Project overview and FAQ
   * - MANIFESTO.rst
     - This file - comprehensive architecture documentation
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

The kernel is written in C with x86 assembly stubs. It provides a Linux-compatible
syscall ABI, preemptive multitasking with per-process paging, VFS, memory
management, and device drivers.

Kernel Entry & Core
-------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - boot.asm
     - Multiboot-compliant entry point, sets up stack
   * - kernel.c
     - kernel_main(): device initialization orchestration
   * - panic.c
     - Kernel panic handler

cpu/Architecture
----------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - gdt.c
     - Global Descriptor Table setup (segments)
   * - idt.c
     - Interrupt Descriptor Table, exception handlers
   * - pic.c
     - 8259 Programmable Interrupt Controller
   * - pit.c
     - 8254 Programmable Interval Timer (scheduler tick)
   * - isr.asm
     - Low-level interrupt stub routines
   * - task.asm
     - Context switch assembly (save/restore registers)

Paging & Memory Management
----------------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - paging.c **(NEW)**
     - **x86 paging implementation:**

       * Physical frame allocator (bitmap-based, 256MB+)
       * Page directory/table management
       * Per-process address spaces
       * Copy-on-write support
       * Page fault handler
   * - memory.c
     - Kernel heap allocator (kmalloc/kfree)

Device Drivers
--------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - console.c
      - VGA text mode + libtsm framebuffer console
   * - serial.c
     - COM1 serial port driver (debug output)
   * - keyboard.c
     - PS/2 keyboard driver with scancode translation
   * - ata.c
     - ATA/IDE disk driver (PIO mode)

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
     - EXT2 filesystem driver (read/write support)
   * - initrd.c
     - Initial RAM disk loader (multiboot module)
   * - vfs.c
     - Virtual Filesystem Switch (unified filesystem interface)

Process/Task Management
-----------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - task.c
     - Scheduler, process creation, context switching with **paging integration:**

       * Per-process page directory creation/destruction
       * ELF loading with paged segments
       * Fork with copy-on-write page directories
       * Context switch via CR3 switching
   * - elf.c
     - ELF32 executable loader (used by task.c)

System Calls & IPC
------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - syscall.c
     - Linux-compatible syscall dispatch table (80+ syscalls)

Power Management
----------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - power.c
     - ACPI parser, shutdown/reboot/suspend support

IPC & Networking
----------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - socket.c
     - Unix domain sockets (AF_UNIX, SOCK_STREAM) for local IPC
   * - pipe.c
     - Anonymous pipe implementation for IPC

Utilities
---------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - string.c
     - Kernel string functions (memcpy, strlen, etc.)
   * - format.c
     - printf-style formatting
   * - shell.c
     - Built-in kernel shell with program autodiscovery

Headers (include/)
==================

Public headers used by both kernel and userland.

Core Types & ABI
----------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - types.h
     - Fixed-width types (u8, u16, u32, i32, etc.)
   * - io.h
     - Port I/O macros (inb, outb, etc.)
   * - multiboot.h
     - Multiboot specification structures
   * - abi/linux.h
     - Linux ABI constants (syscalls, errno, structs)

Paging Header **(NEW)**
-----------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - paging.h
     - **Paging subsystem interface:**

       * Page table/directory structures
       * Frame allocator API
       * Page mapping operations
       * TLB/CR3 operations (inline assembly)
       * Page fault error codes
       * Copy-on-write flags
       * Demand paging support
       * Memory pressure handling

Additional Headers
-------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - signal.h
     - Signal handling definitions (included from task.h)

Kernel-Only Headers
-------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - ata.h
     - ATA driver interface
   * - blkdev.h
     - Block device structures
   * - console.h
     - Console output API
   * - elf.h
     - ELF format definitions
   * - ext2.h
     - EXT2 filesystem structures
   * - gdt.h
     - GDT setup interface
   * - idt.h
     - IDT/interrupt interface
   * - initrd.h
     - Initrd loader interface
   * - keyboard.h
     - Keyboard scancode constants
   * - mbr.h
     - Master Boot Record structures
   * - memory.h
     - Memory allocator interface
   * - panic.h
     - Panic macro
   * - pic.h
     - PIC interface
   * - pipe.h
     - Pipe structures and API
   * - pit.h
     - PIT timer interface
   * - power.h
     - Power management interface
   * - serial.h
     - Serial driver interface
   * - shell.h
     - Shell entry point
   * - socket.h
     - Socket structures and API
   * - string.h
     - String function prototypes
   * - syscall.h
     - Syscall number definitions
   * - task.h
     - Task/process structures, **now includes page_directory**
   * - vfs.h
     - VFS node structures and operations

Userspace (user/)
=================

Native OHOS programs using the syscall ABI.

Programs
--------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - hello.c
     - Test program demonstrating userland execution
   * - sh.c
     - Simple shell (command interpreter)
   * - toolbox.c
     - System utility (cp, mv, rm, mkdir, cat, etc.)
   * - uname.c
     - Print system information

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

Userspace Library (user/lib/)
-----------------------------

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - start.c
     - _start() entry point, argument setup, calls main()
   * - syscall.c
     - Syscall wrapper implementations
   * - syscall.h
     - Syscall function prototypes
   * - runtime.c
     - C runtime support (malloc/free via sbrk, string functions, etc.)
   * - runtime.h
     - Runtime library headers
   * - newlib-gloss.c
     - NewLib "gloss" layer (low-level OS bindings)
   * - xnx/xnx.c
     - XNX client library (connect, surfaces, buffers, commit)
   * - xnx/xnx.h
     - XNX client library header

Build System (tools/)
=====================

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - build_initrd.sh
     - Create initial ramdisk (cpio archive)
   * - create_disk.sh
     - Create raw disk image
   * - ensure_meson.sh
     - Setup Python virtualenv with Meson
   * - meson-requirements.txt
     - Python dependencies for Meson
   * - mkramdisk.py
     - Python ramdisk generator
   * - populate_disk.sh
     - Populate disk image with root filesystem
   * - rootfs_manifest.sh
     - Define files to include in rootfs

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

The toolchain wraps system GCC with:

* --sysroot for ports sysroot
* -ffreestanding -nostdlib
* i686-elf target

Ports (ports/)
==============

Third-party software ports to OHOS.

C Library
---------

.. list-table::
   :header-rows: 1

   * - Directory
     - Description
   * - newlib/
     - NewLib C library port (76 files)

Display Stack
-------------

.. list-table::
   :header-rows: 1

   * - Directory
     - Description
   * - pixman/
     - Pixel manipulation library for software compositing
   * - xnx/
     - XNX protocol compositor and demo build scripts
   * - zlib/
     - Compression library
   * - libsha1/
     - SHA1 implementation (custom, 3 files)

Applications
------------

.. list-table::
   :header-rows: 1

   * - Directory
     - Description
   * - fastfetch/
     - System info display tool (like neofetch)

Resources (assets/)
===================

.. list-table::
   :header-rows: 1

   * - Path
     - Description
   * - etc/motd.txt
     - Message of the day (displayed on boot)
   * - etc/os-release
     - OS identification (NAME, VERSION, etc.)
   * - etc/xdg/fastfetch/
     - Fastfetch configuration files
Boot (grub/)
============

.. list-table::
   :header-rows: 1

   * - File
     - Description
   * - grub.cfg
     - GRUB2 boot menu configuration

================================================================================
                              SYSTEM ARCHITECTURE
================================================================================

1. Boot Process
===============

::

   GRUB (on ISO) → boot.asm → kernel_main()

1. GRUB loads kernel.bin at 1MB (per linker.ld)
2. boot.asm sets up stack, zeroes BSS, calls kernel_main()
3. kernel_main() initializes subsystems:

   - console (early output)
   - GDT (segmentation)
   - PIC (interrupt controller)
   - IDT (interrupt handlers)
   - PIT (timer for scheduling)
   - Keyboard
   - Memory manager
   - **Paging: Initialize page tables and frame allocator (paging not enabled yet — virtual == physical)**
   - Initrd (ramdisk)
   - VFS (mounts initrd at /)
   - Task scheduler
   - Power management
   - Shell or userspace init

2. Memory Layout
================

Physical Memory
---------------

::

   0x00000000 - 0x0009FFFF: Low memory (reserved)
   0x000A0000 - 0x000FFFFF: VGA/ROM (reserved)
   0x00100000 - kernel_end:   Kernel code/data (identity mapped)
   kernel_end - 0x02FF0000:   Kernel heap (grows upward)
   0x03000000+:               User ELF loading (48MB+ — above kernel heap)

.. note::
   Paging subsystem is fully initialized (page tables, frame allocator, CR3
   set) but **paging is not enabled on the CPU**. Virtual addresses equal
   physical addresses. USER_BASE at 0x03000000 was chosen specifically to
   avoid physical overlap with the kernel heap (ends at ~0x02FF0000).

Virtual Address Space (Per Process)
-----------------------------------

::

   0x00000000 - 0x01000000:   Unmapped (null page guard)
   0x01000000 - 0x02000000:   Reserved
   0x03000000:                User code/data (ELF loaded here)
   0x03000000 - 0x03C00000:   Heap (grows via brk)
   0x03C00000 - 0x03F00000:   mmap region
   0x03F00000 - 0x03FF0000:   Stack (grows down)
   0x03FF0000 - 0xC0000000:   Unmapped
   0xC0000000 - 0xFFFFFFFF:   **Kernel space (higher-half, shared)**

Paging Architecture
-------------------

Each process has its own page directory. Page tables are built and CR3 is
set during context switches, but since paging is not enabled the CPU ignores
all page table entries. The infrastructure is wired and ready for when
paging is turned on.

* **User space (0-3GB):** Private to each process (structurally, though
  currently ignored by the CPU)
* **Kernel space (3-4GB):** Shared across all processes (same reason)

3. Process Model
================

Preemptive Multitasking
-----------------------

.. list-table::
   :header-rows: 1

   * - Component
     - Implementation
   * - Scheduler
     - Round-robin, 100Hz timer tick
   * - Context Switch
     - CR3 register switch (infrastructure in place, CPU ignores without paging)
   * - Address Space
     - Per-process page directory (prepared for paging)
   * - Kernel Entry
     - Syscalls via interrupt 0x80
   * - Kernel Exit
     - iretd to user space

Process Creation
----------------

**Fork:**

1. Create new page directory with COW entries
2. Share physical pages between parent and child
3. Page fault handler (ready but unused since paging is off)

**Exec:**

1. Create new page directory
2. Load ELF segments (uses physical addresses — no paging)
3. Map stack pages
4. Switch to new page directory

Process State
-------------

Each task_slot_t contains:

* registers_t regs (saved CPU state)
* task_state_t state (open files, mappings, brk)
* page_directory_t *page_directory
* u32 page_directory_phys

4. Syscall Interface
====================

Linux ABI-compatible syscall numbers from abi/linux.h.

**Key syscall groups:**

* File: open, read, write, close, lseek, stat, mkdir, unlink
* Process: fork, execve, exit, waitpid, getpid
* Memory: brk, mmap2, munmap (now allocate physical pages)
* IPC: pipe, socket, bind, connect, send, recv
* Time: clock_gettime, nanosleep, gettimeofday
* OHOS-specific: spawn, yield (400+)

5. Filesystem Stack
===================

VFS (Virtual File System)
-------------------------

Path resolution → VFS node lookup → Filesystem driver

Supported filesystems:

* **initrd:** Initial ramdisk (cpio), read-only, populated at build
* **ext2:** Full read/write filesystem on disk image
* **devfs:** Device files (/dev/null, /dev/zero, etc.)

Block layer: VFS → BlockDev → ATA driver → Disk

6. Graphics Stack
=================

Console
-------

VGA text mode (early) → libtsm framebuffer (GUI mode) → Serial fallback

XNX Display Protocol
--------------------

* Minimal display protocol using Unix domain sockets (AF_UNIX, SOCK_STREAM)
* Compositor runs as background userspace process (/bin/xnx-compositor)
* Clients connect to /tmp/xnx.sock, create surfaces, write pixel buffers
* Uses pixman for software compositing (PIXMAN_OP_OVER)
* Wire protocol: fixed-header + payload messages (create surface, write
  buffer, commit, set geometry, set title)
* Demo client (/bin/xnx-demo) draws animated gradients as a test

================================================================================
                             PAGING SUBSYSTEM
================================================================================

Overview
--------

.. note::
   The paging subsystem is fully wired — page tables exist, the frame
   allocator runs, CR3 is set during context switches — but
   ``paging_enable()`` is never called. The CPU runs with paging
   disabled, so virtual = physical. This means user ELF loading at
   USER_BASE writes to physical RAM at that address, which is why
   USER_BASE (0x03000000) was chosen to sit above the kernel heap
   (ends at ~0x02FF0000).

* Physical frame allocator (bitmap-based, from 48MB+)
* Per-process page directories
* Page mapping/unmapping operations
* Copy-on-write page table cloning
* Page fault handler (ready but unused)
* Higher-half kernel mappings (3GB+ shared)

Key Components
--------------

Physical Frame Allocator
~~~~~~~~~~~~~~~~~~~~~~~~~

Bitmap-based allocator tracking used/free physical frames starting at 48MB+.

Page Directory Management
~~~~~~~~~~~~~~~~~~~~~~~~~~

Each process gets its own page directory. Kernel entries (768-1023) are
shared; user entries (0-767) are private. ``page_directory_switch`` and
``paging_set_cr3`` are wired in but ineffective without paging enabled.

Memory-Related Syscalls
-----------------------

brk, mmap, and munmap work through ``paging_alloc_user_page`` /
``paging_free_user_page``. Since paging is off, physical == virtual, so
the allocated physical addresses equal the requested virtual ones.

================================================================================
                           FILE COUNT SUMMARY
================================================================================

Source Code
-----------

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - Kernel (src/)
     - 28 files (C + ASM)
   * - Headers (include/)
     - 31 files
   * - Userspace (user/)
     - 6 native programs + XNX compositor + XNX demo + library

Build System
------------

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - Makefile
     - 1 file (295 lines)
   * - Linker scripts
     - 2 files
   * - Tool scripts
     - 11 files

Ports
-----

Port directories with build scripts for NewLib, pixman, fastfetch, and XNX.

Configuration
-------------

.. list-table::
   :header-rows: 1

   * - Component
     - Files
   * - GRUB config
     - 1 file
   * - Assets
     - ~300 files (mostly XKB data)

Excluded (Git Submodules)
-------------------------

.. list-table::
   :header-rows: 1

   * - Directory
     - Approximate Files
   * - user/lib/fastfetch/
     - ~1422 files
   * - user/lib/newlib-cygwin/
     - ~7008 files
   * - user/lib/pixman/
     - ~161 files
   * - user/lib/SDP/
     - Upstream Samsara Display Protocol tree (unused, replaced by XNX)
   * - user/lib/zlib/
     - ~259 files
   * - user/lib/libtsm/
      - ~32 files

================================================================================
                              BUILD PIPELINE
================================================================================

1. **Kernel:** C/ASM → GCC → LD (linker.ld) → kernel.bin
2. **Userlib:** C → GCC → LD (user.ld) → static library
3. **User programs:** C → link with userlib → .elf
4. **Initrd:** Collect user .elf files + assets → cpio archive
5. **Ports:** Download sources → patch → cross-compile → sysroot
6. **ISO:** kernel.bin + initrd.bin + GRUB config → grub-mkrescue

================================================================================
                              END OF MANIFESTO
================================================================================
