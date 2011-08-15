#include <stdio.h>
#include <assert.h>
#include "sched.h"
#include "proc.h"
#include "intr.h"
#include "asmops.h"
#include "config.h"

#define EMPTY(q)	((q)->head == 0)

struct proc_list {
	struct process *head, *tail;
};

static void idle_proc(void);
static void ins_back(struct proc_list *list, struct process *proc);
static void ins_front(struct proc_list *list, struct process *proc);
static void remove(struct proc_list *list, struct process *proc);
static int hash_addr(void *addr);

static struct proc_list runq;
static struct proc_list zombieq;

#define HTBL_SIZE	101
static struct proc_list wait_htable[HTBL_SIZE];


void schedule(void)
{
	disable_intr();

	if(EMPTY(&runq)) {
		if(!get_current_proc()) {
			/* we're already in the idle process, don't reenter it
			 * or you'll fill up the stack very quickly.
			 */
			return;
		}

		idle_proc();
		return;
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

	/* always enter context_switch with interrupts disabled */
	context_switch(runq.head->id);
}

void add_proc(int pid)
{
	int istate;
	struct process *proc;

	istate = get_intr_state();
	disable_intr();

	proc = get_process(pid);

	ins_back(&runq, proc);
	proc->state = STATE_RUNNABLE;

	set_intr_state(istate);
}

/* block the process until we get a wakeup call for address ev */
void wait(void *wait_addr)
{
	struct process *p;
	int hash_idx;

	disable_intr();

	p = get_current_proc();
	assert(p);

	/* remove it from the runqueue ... */
	remove(&runq, p);

	/* and place it in the wait hash table based on sleep_addr */
	hash_idx = hash_addr(wait_addr);
	ins_back(wait_htable + hash_idx, p);

	p->state = STATE_BLOCKED;
	p->wait_addr = wait_addr;

	/* call the scheduler to give time to another process */
	schedule();
}

/* wake up all the processes sleeping on this address */
void wakeup(void *wait_addr)
{
	int hash_idx;
	struct process *iter;
	struct proc_list *list;

	hash_idx = hash_addr(wait_addr);
	list = wait_htable + hash_idx;

	iter = list->head;
	while(iter) {
		if(iter->wait_addr == wait_addr) {
			/* found one, remove it, and make it runnable */
			struct process *p = iter;
			iter = iter->next;

			remove(list, p);
			p->state = STATE_RUNNABLE;
			ins_back(&runq, p);
		} else {
			iter = iter->next;
		}
	}
}

static void idle_proc(void)
{
	/* make sure we send any pending EOIs if needed.
	 * end_of_irq will actually check if it's needed first.
	 */
	struct intr_frame *ifrm = get_intr_frame();
	end_of_irq(INTR_TO_IRQ(ifrm->inum));

	set_current_pid(0);

	/* make sure interrupts are enabled before halting */
	while(EMPTY(&runq)) {
		enable_intr();
		halt_cpu();
		disable_intr();
	}
}


/* list operations */
static void ins_back(struct proc_list *list, struct process *proc)
{
	if(EMPTY(list)) {
		list->head = proc;
	} else {
		list->tail->next = proc;
	}

	proc->next = 0;
	proc->prev = list->tail;
	list->tail = proc;
}

static void ins_front(struct proc_list *list, struct process *proc)
{
	if(EMPTY(list)) {
		list->tail = proc;
	} else {
		list->head->prev = proc;
	}

	proc->next = list->head;
	proc->prev = 0;
	list->head = proc;
}

static void remove(struct proc_list *list, struct process *proc)
{
	if(proc->prev) {
		proc->prev->next = proc->next;
	}
	if(proc->next) {
		proc->next->prev = proc->prev;
	}
	if(list->head == proc) {
		list->head = proc->next;
	}
	if(list->tail == proc) {
		list->tail = proc->prev;
	}
}

static int hash_addr(void *addr)
{
	return (uint32_t)addr % HTBL_SIZE;
}
