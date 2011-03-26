	.text
/* enable_paging(void)
 * sets the cr0 bit 31 which enables page translation */
	.globl enable_paging
enable_paging:
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0
	ret

/* set_pgdir_addr(uint32_t addr)
 * sets the address of the page directory by writing to cr3, which
 * also results in a TLB flush. */
	.globl set_pgdir_addr
set_pgdir_addr:
	movl 4(%esp), %eax
	movl %eax, %cr3
	ret

/* get_fault_addr(void)
 * returns the contents of control register 2, which provides
 * the faulting address during a page fault exception
 */
	.globl get_fault_addr
get_fault_addr:
	movl %cr2, %eax
	ret
