#ifndef INTR_H_
#define INTR_H_

#include <inttypes.h>
#include "asmops.h"

/* offset used to remap IRQ numbers (+32) */
#define IRQ_OFFSET		32
/* conversion macros between IRQ and interrupt numbers */
#define IRQ_TO_INTR(x)	((x) + IRQ_OFFSET)
#define INTR_TO_IRQ(x)	((x) - IRQ_OFFSET)
/* checks whether a particular interrupt is an remapped IRQ */
#define IS_IRQ(n)	((n) >= IRQ_OFFSET && (n) < IRQ_OFFSET + 16)

/* structure used to pass the interrupt stack frame from the
 * entry points to the C dispatch function.
 */
struct intr_frame {
	/* registers pushed by pusha in intr_entry_* */
	struct registers regs;
	/* interrupt number and error code pushed in intr_entry_* */
	uint32_t inum, err;
	/* pushed by CPU during interrupt entry */
	uint32_t eip, cs, eflags;
	/* pushed by CPU during interrupt entry from user space */
	uint32_t esp, ss;
} __attribute__ ((packed));



typedef void (*intr_func_t)(int);


void init_intr(void);

struct intr_frame *get_intr_frame(void);

void interrupt(int intr_num, intr_func_t func);

/* defined in intr-asm.S */
int get_intr_state(void);
void set_intr_state(int s);

void intr_ret(struct intr_frame ifrm);

void end_of_irq(int irq);

#endif	/* INTR_H_ */
