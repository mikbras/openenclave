.text
.global __cp_begin
.hidden __cp_begin
.global __cp_end
.hidden __cp_end
.global __cp_cancel
.hidden __cp_cancel
.hidden __cancel
.global __syscall_cp_asm
.hidden __syscall_cp_asm
.type   __syscall_cp_asm,@function
__syscall_cp_asm:

__cp_begin:
	mov (%rdi),%eax
	test %eax,%eax
	jnz __cp_cancel
        mov %rsi, %rdi
        mov %rdx, %rsi
        mov %rcx, %rdx
        mov %r8, %rcx
        mov %r9, %r8
        mov 8(%rsp),%r9
        mov 16(%rsp),%eax
        mov %eax, 8(%rsp)
        call posix_syscall
__cp_end:
	ret
__cp_cancel:
	jmp __cancel
