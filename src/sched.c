#include <stdio.h>
#include "sched.h"
#include "proc.h"
#include "intr.h"
#include "asmops.h"
#include "config.h"

#define EMPTY(q)	((q).head == 0)

struct proc_list {
	struct process *head, *tail;
};

static void ins_back(struct proc_list *q, struct process *proc);
static void ins_front(struct proc_list *q, struct process *proc);
static void remove(struct proc_list *q, struct process *proc);

static struct proc_list runq;
static struct proc_list waitq;
static struct proc_list zombieq;

void schedule(void)
{
	disable_intr();

	if(EMPTY(runq)) {
		/* idle "process".
		 * make sure interrupts are enabled before halting
		 */
		enable_intr();
		halt_cpu();
		printf("fuck you!\n");
		/* this won't return, it'll just wake up in an interrupt later */
	}

	/* if the current process exhausted its timeslice,
	 * move it to the back of the queue.
	 */
	if(runq.head->ticks_left <= 0) {
		if(runq.head->next) {
			struct process *proc = runq.head;
			remove(&runq, proc);
			ins_back(&runq, proc);
		}

		/* start a new timeslice */
		runq.head->ticks_left = TIMESLICE_TICKS;
	}

	/* no need to re-enable interrupts, they will be enabled with the iret */
	context_switch(runq.head->id);
}

int add_proc(int pid, enum proc_state state)
{
	int istate;
	struct proc_list *q;
	struct process *proc;

	istate = get_intr_state();
	disable_intr();

	proc = get_process(pid);

	q = state == STATE_RUNNING ? &runq : &waitq;

	ins_back(q, proc);
	proc->state = state;

	set_intr_state(istate);
	return 0;
}

int block_proc(int pid)
{
	int istate;
	struct process *proc = get_process(pid);

	if(proc->state != STATE_RUNNING) {
		printf("block_proc: process %d not running\n", pid);
		return -1;
	}

	istate = get_intr_state();
	disable_intr();

	remove(&runq, proc);
	ins_back(&waitq, proc);
	proc->state = STATE_BLOCKED;

	set_intr_state(istate);
	return 0;
}

int unblock_proc(int pid)
{
	int istate;
	struct process *proc = get_process(pid);

	if(proc->state != STATE_BLOCKED) {
		printf("unblock_proc: process %d not blocked\n", pid);
		return -1;
	}

	istate = get_intr_state();
	disable_intr();

	remove(&waitq, proc);
	ins_back(&runq, proc);
	proc->state = STATE_RUNNING;

	set_intr_state(istate);
	return 0;
}


static void ins_back(struct proc_list *q, struct process *proc)
{
	if(EMPTY(*q)) {
		q->head = proc;
	} else {
		q->tail->next = proc;
	}

	proc->next = 0;
	proc->prev = q->tail;
	q->tail = proc;
}

static void ins_front(struct proc_list *q, struct process *proc)
{
	if(EMPTY(*q)) {
		q->tail = proc;
	} else {
		q->head->prev = proc;
	}

	proc->next = q->head;
	proc->prev = 0;
	q->head = proc;
}

static void remove(struct proc_list *q, struct process *proc)
{
	if(proc->prev) {
		proc->prev->next = proc->next;
	}
	if(proc->next) {
		proc->next->prev = proc->prev;
	}
	if(q->head == proc) {
		q->head = proc->next;
	}
	if(q->tail == proc) {
		q->tail = proc->prev;
	}
}
