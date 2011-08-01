#define ASM
#include <syscall.h>

	.text
	.globl test_proc
test_proc:
	/* --- print a message --- */
	movl $SYS_HELLO, %eax
	int $SYSCALL_INT

	/* --- sleep for 5 seconds --- */
	movl $SYS_SLEEP, %eax
	movl $5, %ebx
	int $SYSCALL_INT

	jmp test_proc

	.globl test_proc_end
test_proc_end:
