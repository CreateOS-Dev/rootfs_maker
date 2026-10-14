.section .text
.code64

.global user_command_start
.global _start
.type user_command_start, @function
.extern main
.extern sys_exit

user_command_start:
_start:
    xor %rbp, %rbp
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    and $-16, %rsp
    call main
    mov %eax, %edi
    call sys_exit

1:
    cli
    hlt
    jmp 1b

.section .note.GNU-stack,"",@progbits
