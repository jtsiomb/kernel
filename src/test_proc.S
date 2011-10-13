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

	jmp infloop

	.globl test_proc_end
test_proc_end:
