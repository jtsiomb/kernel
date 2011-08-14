	.text
	/* switch_stack(uint32_t new_stack)
     * switches to the new stack and returns the old stack pointer
     */
	.globl switch_stack
switch_stack:
	movl %esp, %eax
	movl 4(%esp), %esp
	ret
