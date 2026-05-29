# OpenHobbyOS (OHOS)

> A minimalistic 32-bit x86 monolithic kernel with a custom userspace ecosystem, designed for educational exploration and lightweight deployment.

---

## Overview

OpenHobbyOS (OHOS) is a from-scratch, monolithic operating system targeting the i386 architecture. Written in C with critical paths in NASM assembly, it emphasizes a small kernel footprint while maintaining compatibility with ported POSIX-like userspace software.

**Key features:**
- Preemptive multitasking with round-robin scheduler
- Linux ABI-compatible syscall dispatch (80+ syscalls via `int 0x80`)
- Per-process page directories with copy-on-write fork (infrastructure wired)
- Modular VFS with initrd (custom cpio-like), EXT2 (read/write), and devfs
- libtsm-based framebuffer console with VT100 emulation
- XNX display protocol — Unix domain socket + pixman compositing
- lwIP TCP/IP stack port with RTL8139 NIC driver
- ACPI power management via uACPI
- Cross-compiled newlib C library (`i686-openhobbyos-elf` toolchain)
- Ports: fastfetch, Doom, XNX compositor/demo

---

## Requirements

- **CPU:** i386 / IA-32 compatible (Pentium or later)
- **RAM:** 200–500 MB
- **Disk:** CD-ROM or IDE disk image

---

## Build Dependencies

```
gcc-multilib (or i686-elf-gcc)
nasm >= 2.15
make, python3 >= 3.8, xorriso, grub-mkrescue, mtools

Optional (ports):
meson, ninja, pkg-config, autoconf, automake, cmake
```

---

## Quick Start

```bash
# Build kernel, userspace, and ISO
make all

# Run in QEMU (GUI)
make run

# Run with serial debug output
make run-debug

# Clean build artifacts
make clean
```

The build produces `build/OpenHobbyOS.iso` — a bootable GRUB2 rescue ISO containing the kernel and initial ramdisk.

---

## Project Structure

| Directory | Purpose |
|-----------|---------|
| `src/` | Kernel source (32 C files + 3 NASM files) |
| `include/` | Kernel and shared headers (28 files) |
| `user/` | Userspace programs and libraries |
| `ports/` | Third-party software build scripts |
| `tools/` | Build and disk image tooling |
| `assets/` | Root filesystem assets (configs, Doom WAD) |
| `grub/` | GRUB2 boot configuration |
| `toolchain/` | Cross-compiler wrappers |

---

## Architecture

### Boot Process

```
GRUB (ISO) → boot.asm → kernel_main()
```

1. GRUB loads kernel.bin at 1MB (via linker.ld)
2. `boot.asm` sets up stack, zeroes BSS, calls `kernel_main()`
3. `kernel_main()` initializes: console → GDT → PIC → IDT → PIT → keyboard → memory → paging → initrd → VFS → task scheduler → power → userspace init

### Memory Layout

```
0x00000000 - 0x0009FFFF: Low memory (reserved)
0x000A0000 - 0x000FFFFF: VGA/ROM (reserved)
0x00100000 - kernel_end:   Kernel code/data (identity mapped)
kernel_end - 0x02FF0000:   Kernel heap
0x03000000+:               User ELF loading (48MB+)
```

Paging subsystem is fully initialized but **paging is not enabled on the CPU** . why ? for debugging purposes mainly. you can enable it and it wil work fine.
### Syscall Interface

Linux ABI-compatible syscall numbers via `int 0x80`. Key groups: file I/O, process (fork/execve/wait), memory (brk/mmap), IPC (pipe, Unix sockets), time, and OHOS-specific (spawn, yield at 400+).

### Filesystem Stack

```
VFS → initrd (read-only, boot) | ext2 (read/write, disk) | devfs (/dev/*)
Block layer: VFS → blkdev → ATA driver → Disk
```

### Graphics

- Early: VGA text mode (80x25)
- Framebuffer: libtsm terminal emulation with VT100 support
- XNX: Unix domain socket display protocol, pixman compositing

---

## Ports

| Port | Description |
|------|-------------|
| newlib | C standard library (cross-compiled) |
| zlib | Compression library |
| pixman | Software pixel compositing |
| fastfetch | System info display |
| lwIP | TCP/IP stack |
| libtsm | Terminal emulator (kernel-linked) |
| uACPI | ACPI implementation (kernel-linked) |
| Doom | Game port via doomgeneric |
| XNX | Display protocol (compositor, client lib, demo) |

---

## License

BSD 3-Clause. See `LICENSE`.
