#ifndef PROC_H_
#define PROC_H_

#include <inttypes.h>
#include "asmops.h"
#include "rbtree.h"

#define MAX_PROC	128

struct context {
	/*struct registers regs;*/	/* saved general purpose registers */
	/*uint32_t instr_ptr;*/		/* saved eip */
	uint32_t stack_ptr;		/* saved esp */
	/*uint32_t flags;*/			/* saved eflags */
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

	int exit_status;

	/* when blocked it's waiting for a wakeup on this address */
	void *wait_addr;

	int ticks_left;

	/* process vm map */
	struct rbtree vmmap;

	/* extends of the process heap, increased by sbrk */

	/* first page of the user stack, extends up to KMEM_START */
	int user_stack_pg;
	/* first page of the kernel stack, (KERN_STACK_SIZE) */
	int kern_stack_pg;

	struct context ctx;

	struct process *child_list;

	struct process *next, *prev;	/* for the scheduler queues */
	struct process *sib_next;		/* for the sibling list */
};

void init_proc(void);

int sys_fork(void);
int sys_exit(int status);
int sys_waitpid(int pid, int *status, int opt);

void context_switch(int pid);

void set_current_pid(int pid);
int get_current_pid(void);
struct process *get_current_proc(void);
struct process *get_process(int pid);

int sys_getpid(void);
int sys_getppid(void);

/* defined in proc-asm.S */
uint32_t get_instr_ptr(void);
uint32_t get_caller_instr_ptr(void);
void get_instr_stack_ptr(uint32_t *iptr, uint32_t *sptr);

#endif	/* PROC_H_ */
