	.data
off:.long 0
seg:.short 0

	.align
	.short 0
gdt_lim:
	.short 0
gdt_addr:
	.long 0

	.text
/* setup_selectors(uint16_t code, uint16_t data)
 * loads the requested selectors to all the selector registers */
	.globl setup_selectors
setup_selectors:
	/* setup data selectors */
	movl 8(%esp), %eax
	movw %ax, %ss
	movw %ax, %es
	movw %ax, %ds
	movw %ax, %gs
	movw %ax, %fs
	/* setup code selector */
	movl 4(%esp), %eax
	movw %ax, (seg)
	movl $ldcs, (off)
	ljmp *off
ldcs:
	ret

/* set_gdt(uint32_t addr, uint16_t limit)
 * loads the GDTR with the new address and limit for the GDT */
	.globl set_gdt
set_gdt:
	movl 4(%esp), %eax
	movl %eax, (gdt_addr)
	movw 8(%esp), %ax
	movw %ax, (gdt_lim)
	lgdt (gdt_lim)	
	ret
