#ifndef PROC_H_
#define PROC_H_

#include <inttypes.h>
#include "asmops.h"

#define MAX_PROC	128

struct context {
	struct registers regs;	/* saved general purpose registers */
	uint32_t instr_ptr;		/* saved eip */
	uint32_t stack_ptr;		/* saved esp */
	uint32_t flags;			/* saved eflags */
	uint32_t pgtbl_paddr;	/* physical address of the page table */
	/* TODO add FPU state */
};


struct process {
	int parent;
	struct context ctx;
};

void init_proc(void);

void context_switch(int pid);

#endif	/* PROC_H_ */
