[BITS 32]

SECTION .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)

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
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

SECTION .note.GNU-stack noalloc noexec nowrite progbits
