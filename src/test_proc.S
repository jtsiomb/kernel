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

	/* --- call getpid ---
	 * we'll use the process id, multiplied by two as a
	 * sleep interval, to avoid having the two test processes
	 * sleep for identical amounts of time.
	 */
	movl $SYS_GETPID, %eax
	int $SYSCALL_INT
	movl %eax, %ebx
	shl $1, %ebx

	/* --- sleep for (pid * 2) seconds --- 
	 * ebx (the argument to sleep) already contains the
	 * correct value
	 */
	movl $SYS_SLEEP, %eax
	int $SYSCALL_INT

	jmp infloop

	.globl test_proc_end
test_proc_end:
