#define ASM
#include <syscall.h>

	.text
	.globl test_proc
test_proc:
	/* fork another process */
	movl $SYS_FORK, %eax
	int $SYSCALL_INT

	/* test copy-on-write by pushing the pid to the stack
	 * then use this value from the stack times 2 as a sleep
     * interval in the loop.
	 */
	movl $SYS_GETPID, %eax
	int $SYSCALL_INT
	push %eax

	/* this will count the iterations */
	xor %ecx, %ecx

infloop:
	/* --- print a message --- */
	movl $SYS_HELLO, %eax
	int $SYSCALL_INT


	/* --- sleep for (pid * 2) seconds --- 
	 * grab the pid from the stack and shift it left to
	 * multiply the pid by 2. Then use that as a sleep interval
	 * in seconds.
	 */
	movl (%esp), %ebx
	shl $1, %ebx
	movl $SYS_SLEEP, %eax
	int $SYSCALL_INT

	inc %ecx

	/* let process 2 quit after 2 iterations */
	cmpl $2, (%esp)
	jne 1f
	cmpl $2, %ecx
	je exit_proc

1:
	jmp infloop

exit_proc:
	movl $SYS_EXIT, %eax
	movl $0, %ebx
	int $SYSCALL_INT

	/* shouldn't reach this, trap otherwise */
	int $3

	.globl test_proc_end
test_proc_end:
