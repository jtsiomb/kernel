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

enum proc_state {
	STATE_RUNNABLE,
	STATE_BLOCKED,
	STATE_ZOMBIE
};


struct process {
	int id, parent;
	enum proc_state state;

	int ticks_left;

	/* extends of the process heap, increased by sbrk */

	/* first page of the user stack, extends up to KMEM_START */
	int stack_start_pg;

	struct context ctx;

	struct process *next, *prev;	/* for the scheduler queues */
};

void init_proc(void);

void context_switch(int pid);

int get_current_pid(void);
struct process *get_current_proc(void);
struct process *get_process(int pid);

#endif	/* PROC_H_ */
