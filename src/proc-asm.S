	.text
	/* switch_stack(uint32_t new_stack, uint32_t *old_stack_ptr)
     * switches to the new stack and returns the old stack pointer, which is
     * also copied to the address passed as the second argument.
     */
	.globl switch_stack
switch_stack:
	movl %esp, %eax		/* old stack in eax */
	movl 8(%esp), %edx
	cmpl $0, %edx		/* if old_stack_ptr is null, skip ahead */
	jz oldp_is_null
	movl %eax, (%edx)	/* otherwise *old_stack_ptr = eax */
oldp_is_null:
	movl 4(%esp), %esp	/* set the new stack */
	ret

	/* get_instr_stack_ptr(uint32_t *eip, uint32_t *esp)
     * returns the current instruction and stack pointers at the same
     * point in execution, so that a newly-forked process with these
     * values will just return from this function and continue on.
	 */
	.globl get_instr_stack_ptr
get_instr_stack_ptr:
	call get_instr_ptr
	movl %eax, 4(%esp)
	movl %esp, 8(%esp)
	ret

	/* get_instr_ptr(void)
	 * returns the address of the next instruction after the call to this function
	 */
	.globl get_instr_ptr
get_instr_ptr:
	movl (%esp), %eax
	ret

	/* get_caller_instr_ptr(void)
	 * returns the address of the next instruction after the call to the function that
	 * called this function.
	 * NOTE: will only work properly when called from a function that uses ebp to point
	 * to its stack frame, which means all of the C functions but pretty much none of
	 * our assembly functions.
	 */
	.globl get_caller_instr_ptr
get_caller_instr_ptr:
	movl 4(%ebp), %eax
	ret

	/* this is where we end up when we first context_switch to a newly forked
	 * process. The interrupt frame is already there, so we just call intr_ret
	 * to return to user space
	 */
	.globl just_forked
just_forked:
	call intr_ret
