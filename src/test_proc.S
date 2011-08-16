#define ASM
#include <syscall.h>

	.text
	.globl test_proc
test_proc:
	/* fork another process */
	movl $SYS_FORK, %eax
	int $SYSCALL_INT

infloop:
	/* --- print a message --- */
	movl $SYS_HELLO, %eax
	int $SYSCALL_INT

	/* --- call getpid --- */
	movl $SYS_GETPID, %eax
	int $SYSCALL_INT
	/* copy the pid to ebx to use as a sleep interval */
	movl %eax, %ebx
	shl $1, %ebx

	/* --- sleep for 5 seconds --- */
	movl $SYS_SLEEP, %eax
	/*movl $3, %ebx*/
	int $SYSCALL_INT

	jmp infloop

	.globl test_proc_end
test_proc_end:
