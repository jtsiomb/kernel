	.data
	.align 4
	.short 0
/* memory reserved for set_idt */
lim:.short 0
addr:.long 0

	.text
/* set_idt(uint32_t addr, uint16_t limit)
 * loads the IDTR with the new address and limit for the IDT */
	.globl set_idt
set_idt:
	movl 4(%esp), %eax
	movl %eax, (addr)
	movw 8(%esp), %ax
	movw %ax, (lim)
	lidt (lim)
	ret

/* get_intr_state()
 * returns 1 if interrutps are enabled, 0 if disabled */
	.globl get_intr_state
get_intr_state:
	pushf
	popl %eax
	shr $9, %eax  /* bit 9 of eflags is IF */
	andl $1, %eax
	ret

/* set_intr_state(int state)
 * enables interrupts if the argument is non-zero, disables them otherwise */
	.globl set_intr_state
set_intr_state:
	cmpl $0, 4(%esp)
	jz 0f
	sti
	ret
0:	cli
	ret


/* interrupt entry with error code macro
 * this macro generates an interrupt entry point for the
 * exceptions which include error codes in the stack frame
 */
	.macro ientry_err n name
	.globl intr_entry_\name
intr_entry_\name:
	pushl $\n
	jmp intr_entry_common
	.endm

/* interrupt entry without error code macro
 * this macro generates an interrupt entry point for the interrupts
 * and exceptions which do not include error codes in the stack frame
 * it pushes a dummy error code (0), to make the stack frame identical
 */
	.macro ientry_noerr n name
	.globl intr_entry_\name
intr_entry_\name:
	pushl $0
	pushl $\n
	jmp intr_entry_common
	.endm

/* common code used by all entry points. calls dispatch_intr()
 * defined in intr.c
 */
	.extern dispatch_intr
intr_entry_common:
	pusha
	call dispatch_intr
	popa
	/* remove error code and intr num from stack */
	add $8, %esp
	iret

/* by including interrupts.h with ASM defined, the macros above
 * are expanded to generate all required interrupt entry points
 */
#define ASM
#include <interrupts.h>
