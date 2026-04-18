.section .text
.globl _start
.type _start, @function

_start:
    movl (%esp), %eax
    leal 4(%esp), %ebx
    leal 8(%esp,%eax,4), %ecx
    pushl %ecx
    pushl %ebx
    pushl %eax
    call main
    pushl %eax
    call exit

1:
    hlt
    jmp 1b
