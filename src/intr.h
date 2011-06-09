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


typedef void (*intr_func_t)(int, uint32_t);


void init_intr(void);

void interrupt(int intr_num, intr_func_t func);

/* defined in intr-asm.S */
int get_intr_state(void);
void set_intr_state(int s);

#endif	/* INTR_H_ */
