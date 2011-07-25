#ifndef PROC_H_
#define PROC_H_

#include <inttypes.h>
#include "asmops.h"

#define MAX_PROC	128

struct context {
	struct registers regs;
	uint32_t instr_ptr;
	uint32_t stack_ptr;
	uint32_t flags;
	uint32_t pgtbl_paddr;
	/* TODO add FPU state */
};


struct process {
	struct context ctx;
};

#endif	/* PROC_H_ */
