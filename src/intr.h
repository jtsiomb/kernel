#ifndef INTR_H_
#define INTR_H_

#include <inttypes.h>
#include "asmops.h"

typedef void (*intr_func_t)(int, uint32_t);


void init_intr(void);

void interrupt(int intr_num, intr_func_t func);

/* defined in intr-asm.S */
int get_intr_state(void);
void set_intr_state(int s);

#endif	/* INTR_H_ */
