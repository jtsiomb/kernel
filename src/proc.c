#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "config.h"
#include "proc.h"
#include "tss.h"
#include "vm.h"
#include "segm.h"
#include "intr.h"
#include "panic.h"
#include "syscall.h"
#include "sched.h"
#include "tss.h"

#define	FLAGS_INTR_BIT	(1 << 9)

static void start_first_proc(void);

/* defined in proc-asm.S */
uint32_t switch_stack(uint32_t new_stack, uint32_t *old_stack);
void just_forked(void);

/* defined in test_proc.S */
void test_proc(void);
void test_proc_end(void);

static struct process proc[MAX_PROC];

/* cur_pid:  pid of the currently executing process.
 *           when we're in the idle process cur_pid will be 0.
 * last_pid: pid of the last real process that was running, this should
 *           never become 0. Essentially this defines the active kernel stack.
 */
static int cur_pid, last_pid;

static struct task_state *tss;


void init_proc(void)
{
	int tss_page;

	/* allocate a page for the task state segment, to make sure
	 * it doesn't cross page boundaries
	 */
	if((tss_page = pgalloc(1, MEM_KERNEL)) == -1) {
		panic("failed to allocate memory for the task state segment\n");
	}
	tss = (struct task_state*)PAGE_TO_ADDR(tss_page);

	/* the kernel stack segment never changes so we might as well set it now
	 * the only other thing that we use in the tss is the kernel stack pointer
	 * which is different for each process, and thus managed by context_switch
	 */
	memset(tss, 0, sizeof *tss);
	tss->ss0 = selector(SEGM_KDATA, 0);

	set_tss((uint32_t)tss);

	/* initialize system call handler (see syscall.c) */
	init_syscall();

	start_first_proc(); /* XXX never returns */
}

static void start_first_proc(void)
{
	struct process *p;
	int proc_size_pg, img_start_pg, stack_pg;
	uint32_t img_start_addr;
	struct intr_frame ifrm;

	/* prepare the first process */
	p = proc + 1;
	p->id = 1;
	p->parent = 0;	/* no parent for init */

	p->ticks_left = TIMESLICE_TICKS;
	p->next = p->prev = 0;

	/* the first process may keep this existing page table */
	p->ctx.pgtbl_paddr = get_pgdir_addr();

	/* allocate a chunk of memory for the process image
	 * and copy the code of test_proc there.
	 */
	proc_size_pg = (test_proc_end - test_proc) / PGSIZE + 1;
	if((img_start_pg = pgalloc(proc_size_pg, MEM_USER)) == -1) {
		panic("failed to allocate space for the init process image\n");
	}
	img_start_addr = PAGE_TO_ADDR(img_start_pg);
	memcpy((void*)img_start_addr, test_proc, proc_size_pg * PGSIZE);
	printf("copied init process at: %x\n", img_start_addr);

	/* allocate the first page of the user stack */
	stack_pg = ADDR_TO_PAGE(KMEM_START) - 1;
	if(pgalloc_vrange(stack_pg, 1) == -1) {
		panic("failed to allocate user stack page\n");
	}
	p->user_stack_pg = stack_pg;

	/* allocate a kernel stack for this process */
	if((p->kern_stack_pg = pgalloc(KERN_STACK_SIZE / PGSIZE, MEM_KERNEL)) == -1) {
		panic("failed to allocate kernel stack for the init process\n");
	}
	/* when switching from user space to kernel space, the ss0:esp0 from TSS
	 * will be used to switch to the per-process kernel stack, so we need to
	 * set it correctly before switching to user space.
	 * tss->ss0 is already set in init_proc above.
	 */
	tss->esp0 = PAGE_TO_ADDR(p->kern_stack_pg) + KERN_STACK_SIZE;


	/* now we need to fill in the fake interrupt stack frame */
	memset(&ifrm, 0, sizeof ifrm);
	/* after the priviledge switch, this ss:esp will be used in userspace */
	ifrm.esp = PAGE_TO_ADDR(stack_pg) + PGSIZE;
	ifrm.ss = selector(SEGM_UDATA, 3);
	/* instruction pointer at the beginning of the process image */
	ifrm.eip = img_start_addr;
	ifrm.cs = selector(SEGM_UCODE, 3);
	/* make sure the user will run with interrupts enabled */
	ifrm.eflags = FLAGS_INTR_BIT;
	/* user data selectors should all be the same */
	ifrm.ds = ifrm.es = ifrm.fs = ifrm.gs = ifrm.ss;

	/* add it to the scheduler queues */
	add_proc(p->id);

	/* make it current */
	set_current_pid(p->id);

	/* build the current vm map */
	cons_vmmap(&p->vmmap);

	/* execute a fake return from interrupt with the fake stack frame */
	intr_ret(ifrm);
}

int fork(void)
{
	int i, pid;
	struct process *p, *parent;

	disable_intr();

	/* find a free process slot */
	/* TODO don't search up to MAX_PROC if uid != 0 */
	pid = -1;
	for(i=1; i<MAX_PROC; i++) {
		if(proc[i].id == 0) {
			pid = i;
			break;
		}
	}

	if(pid == -1) {
		/* process table full */
		return -EAGAIN;
	}


	p = proc + pid;
	parent = get_current_proc();

	/* allocate a kernel stack for the new process */
	if((p->kern_stack_pg = pgalloc(KERN_STACK_SIZE / PGSIZE, MEM_KERNEL)) == -1) {
		return -EAGAIN;
	}
	p->ctx.stack_ptr = PAGE_TO_ADDR(p->kern_stack_pg) + KERN_STACK_SIZE;
	/* we need to copy the current interrupt frame to the new kernel stack so
	 * that the new process will return to the same point as the parent, just
	 * after the fork syscall.
	 */
	p->ctx.stack_ptr -= sizeof(struct intr_frame);
	memcpy((void*)p->ctx.stack_ptr, get_intr_frame(), sizeof(struct intr_frame));
	/* child's return from fork returns 0 */
	((struct intr_frame*)p->ctx.stack_ptr)->regs.eax = 0;

	/* we also need the address of just_forked in the stack, so that switch_stacks
	 * called from context_switch, will return to just_forked when we first switch
	 * to a newly forked process. just_forked then just calls intr_ret to return to
	 * userspace with the already constructed interrupt frame (see above).
	 */
	p->ctx.stack_ptr -= 4;
	*(uint32_t*)p->ctx.stack_ptr = (uint32_t)just_forked;

	/* initialize the rest of the process structure */
	p->id = pid;
	p->parent = parent->id;
	p->next = p->prev = 0;

	/* will be copied on write */
	p->user_stack_pg = parent->user_stack_pg;

	/* clone the parent's virtual memory */
	clone_vm(p, parent, CLONE_COW);

	/* done, now let's add it to the scheduler runqueue */
	add_proc(p->id);

	return pid;
}

void context_switch(int pid)
{
	static struct process *prev, *new;

	assert(get_intr_state() == 0);
	assert(pid > 0);
	assert(last_pid > 0);

	prev = proc + last_pid;
	new = proc + pid;

	if(last_pid != pid) {
		set_current_pid(new->id);

		/* switch to the new process' address space */
		set_pgdir_addr(new->ctx.pgtbl_paddr);

		/* make sure we'll return to the correct kernel stack next time
		 * we enter from userspace
		 */
		tss->esp0 = PAGE_TO_ADDR(new->kern_stack_pg) + KERN_STACK_SIZE;

		/* push all registers onto the stack before switching stacks */
		push_regs();

		/* XXX: when switching to newly forked processes this switch_stack call
		 * WILL NOT RETURN HERE. It will return to just_forked instead. So the
		 * rest of this function will not run.
		 */
		switch_stack(new->ctx.stack_ptr, &prev->ctx.stack_ptr);

		/* restore registers from the new stack */
		pop_regs();
	} else {
		set_current_pid(new->id);
	}
}


void set_current_pid(int pid)
{
	cur_pid = pid;
	if(pid > 0) {
		last_pid = pid;
	}
}

int get_current_pid(void)
{
	return cur_pid;
}

struct process *get_current_proc(void)
{
	return cur_pid > 0 ? &proc[cur_pid] : 0;
}

struct process *get_process(int pid)
{
	return &proc[pid];
}
