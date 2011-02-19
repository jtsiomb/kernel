#ifndef INTR_H_
#define INTR_H_

#include <inttypes.h>

typedef void (*intr_func_t)(int, uint32_t);


void init_intr(void);

void interrupt(int intr_num, intr_func_t func);

#endif	/* INTR_H_ */
