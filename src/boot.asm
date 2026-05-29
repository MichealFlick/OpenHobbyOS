[BITS 32]

%define MULTIBOOT_HEADER_MAGIC    0x1BADB002
%define MULTIBOOT_HEADER_FLAGS    0x00000007
%define MULTIBOOT_HEADER_CHECKSUM -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
%define MULTIBOOT_MODE_TYPE       0
%define MULTIBOOT_MODE_WIDTH      1024
%define MULTIBOOT_MODE_HEIGHT     768
%define MULTIBOOT_MODE_DEPTH      32

SECTION .multiboot
align 4
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_HEADER_CHECKSUM
    ; Ask GRUB for a linear 32-bit framebuffer so the console can own the screen.
    dd MULTIBOOT_MODE_TYPE
    dd MULTIBOOT_MODE_WIDTH
    dd MULTIBOOT_MODE_HEIGHT
    dd MULTIBOOT_MODE_DEPTH

SECTION .bss
align 16
global stack_bottom
stack_bottom:
    resb 32768
global stack_top
stack_top:

SECTION .text
global start
extern kernel_main

start:
    cli
    ; explicitly disable PAE (CR4 bit 5) — GRUB might leave it set
    mov ecx, cr4
    and ecx, ~0x20
    mov cr4, ecx
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

SECTION .note.GNU-stack noalloc noexec nowrite progbits
