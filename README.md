# OpenHobbyOS (OHOS)

> A minimalistic, reproducible 32-bit x86 monolithic kernel with a custom userspace ecosystem, designed for educational exploration and lightweight embedded deployment.

![Screenshot placeholder — OHOS boot sequence with XNX compositor and mksh session](assets/screenshot.png)

---

## Overview

OpenHobbyOS (OHOS) is a from-scratch, monolithic operating system targeting the i386 architecture (Pentium-class or later). Written primarily in C with critical paths in NASM assembly, OHOS emphasizes a minimal kernel footprint (<256 KiB compressed) while maintaining compatibility with ported POSIX-like userspace software. The kernel implements a preemptive multitasking scheduler, demand-paged virtual memory with strict kernel/user isolation, and a modular VFS layer. Userspace is built around a custom SysV-IPC-inspired syscall ABI (`int 0x80`), a Unix-domain-socket-backed display protocol (XNX), and a cross-compiled newlib-based toolchain (`i686-openhobbyos-elf`).

The project originated as a personal hobby kernel circa 2011. In 2024, it merged with another userspace project that was made originally for the linux kernel (XANAX userspace framework), introducing a composable graphics stack and reproducible build infrastructure. The full source tree was released publicly under BSD-2-Clause in 2025 with deterministic build support.

---

## Target Platform & Hardware Requirements

you generally require a proper IA-32 / 86x / i386 CPU. as for ram. anything ranging from around 200 to 500 MB are fine.

---

## Build Environment

OHOS requires a Linux host with a cross-compilation toolchain targeting `i386-elf` or a native `gcc -m32` configuration with multiboot headers.

### Host Dependencies

```bash
# Toolchain
i686-elf-gcc | gcc-multilib (with -m32, -march=i386, -mno-sse)
i686-elf-ld  | ld.bfd (elf_i386 output format)
nasm >= 2.15                              # NASM syntax for bootloaders/entry

# Build utilities
make, python3 >= 3.8, xorriso, grub-mkrescue, mtools

# Optional (needed for ports)
meson, ninja, pkg-config, autoconf, automake