#define MAGIC		0x1badb002
/* flags with bit 1 set means we need memory info */
#define FLAGS		2
#define STACK_SIZE	0x4000

	.text
	.align 4
	
	/* multiboot header */
	.long MAGIC
	.long FLAGS
	.long -(MAGIC + FLAGS)	/* checksum */

	.globl kentry
kentry:
	/* setup a temporary kernel stack */
	movl $(stack + STACK_SIZE), %esp
	/* reset eflags register */
	pushl $0
	popf
	/* call the kernel main function. ebx points to the
     * multiboot information structure */
	push %ebx
	call kmain
	/* we dropped out of main, halt the CPU */
	cli
	hlt

	/* space for the temporary kernel stack */
	.comm stack, STACK_SIZE
