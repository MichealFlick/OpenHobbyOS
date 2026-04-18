[BITS 32]

SECTION .text

global task_enter_user
global task_return_to_kernel

extern task_saved_esp
extern task_exit_code

task_enter_user:
    mov eax, [esp + 4]
    mov edx, [esp + 8]
    mov [task_saved_esp], esp
    cli
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push dword 0x23
    push edx
    pushfd
    or dword [esp], 0x200
    push dword 0x1B
    push eax
    iretd

task_return_to_kernel:
    mov eax, [esp + 4]
    mov [task_exit_code], eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, [task_saved_esp]
    sti
    ret

SECTION .note.GNU-stack noalloc noexec nowrite progbits
