[BITS 32]

SECTION .text

global task_resume_user
global task_return_to_kernel

extern task_saved_esp
extern task_saved_ebx
extern task_saved_esi
extern task_saved_edi
extern task_saved_ebp
extern task_exit_code

%define REG_EDI     0
%define REG_ESI     4
%define REG_EBP     8
%define REG_EBX     16
%define REG_EDX     20
%define REG_ECX     24
%define REG_EAX     28
%define REG_EIP     56
%define REG_CS      60
%define REG_EFLAGS  64
%define REG_USERESP 68
%define REG_SS      72

task_resume_user:
    mov [task_saved_esp], esp
    mov [task_saved_ebx], ebx
    mov [task_saved_esi], esi
    mov [task_saved_edi], edi
    mov [task_saved_ebp], ebp
    mov ebx, [esp + 4]
    cli

    push dword [ebx + REG_SS]
    push dword [ebx + REG_USERESP]
    push dword [ebx + REG_EFLAGS]
    push dword [ebx + REG_CS]
    push dword [ebx + REG_EIP]

    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov cx, 0x33
    mov gs, cx

    mov edi, [ebx + REG_EDI]
    mov esi, [ebx + REG_ESI]
    mov ebp, [ebx + REG_EBP]
    mov edx, [ebx + REG_EDX]
    mov ecx, [ebx + REG_ECX]
    mov eax, [ebx + REG_EAX]
    mov ebx, [ebx + REG_EBX]
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
    mov ebx, [task_saved_ebx]
    mov esi, [task_saved_esi]
    mov edi, [task_saved_edi]
    mov ebp, [task_saved_ebp]
    sti
    mov eax, [task_exit_code]
    ret

SECTION .note.GNU-stack noalloc noexec nowrite progbits
