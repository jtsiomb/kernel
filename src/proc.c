#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "proc.h"
#include "tss.h"
#include "vm.h"
#include "segm.h"
#include "intr.h"
#include "panic.h"
#include "syscall.h"
#include "sched.h"

#define	FLAGS_INTR_BIT	9

/* defined in test_proc.S */
void test_proc(void);
void test_proc_end(void);

static struct process proc[MAX_PROC];
static int cur_pid;

void init_proc(void)
{
	int proc_size_pg, img_start_pg, stack_pg;
	void *img_start;
	cur_pid = 0;

	init_syscall();

	/* prepare the first process */
	proc[1].id = 1;
	proc[1].parent = 0;

	/* allocate a chunk of memory for the process image
	 * and copy the code of test_proc there.
	 * (should be mapped at a fixed address)
	 */
	proc_size_pg = (test_proc_end - test_proc) / PGSIZE + 1;
	if((img_start_pg = pgalloc(proc_size_pg, MEM_USER)) == -1) {
		panic("failed to allocate space for the init process image\n");
	}
	img_start = (void*)PAGE_TO_ADDR(img_start_pg);
	memcpy(img_start, test_proc, proc_size_pg * PGSIZE);

	printf("copied init process at: %x\n", (unsigned int)img_start);

	/* instruction pointer at the beginning of the process image */
	proc[1].ctx.instr_ptr = (uint32_t)img_start;

	/* allocate the first page of the process stack */
	stack_pg = ADDR_TO_PAGE(KMEM_START) - 1;
	if(pgalloc_vrange(stack_pg, 1) == -1) {
		panic("failed to allocate user stack page\n");
	}
	proc[1].ctx.stack_ptr = PAGE_TO_ADDR(stack_pg) + PGSIZE;
	proc[1].stack_start_pg = stack_pg;

	/* create the virtual address space for this process */
	/*proc[1].ctx.pgtbl_paddr = clone_vm();*/

	/* we don't need the image and the stack in this address space */
	/*unmap_page_range(img_start_pg, proc_size_pg);
	pgfree(img_start_pg, proc_size_pg);

	unmap_page(stack_pg);
	pgfree(stack_pg, 1);*/

	/* add it to the scheduler queues */
	add_proc(1, STATE_RUNNABLE);

	/* switch to the initial process, this never returns */
	context_switch(1);
}


void context_switch(int pid)
{
	struct context *ctx;
	struct intr_frame ifrm;
	struct intr_frame *cur_frame = get_intr_frame();

	assert(0);

	if(cur_pid == pid) {
		assert(cur_frame);
		intr_ret(*cur_frame);
	}

	ctx = &proc[pid].ctx;
	cur_pid = pid;

	ifrm.inum = ifrm.err = 0;

	ifrm.regs = ctx->regs;
	ifrm.eflags = ctx->flags | (1 << FLAGS_INTR_BIT);

	ifrm.eip = ctx->instr_ptr;
	ifrm.cs = selector(SEGM_KCODE, 0);	/* XXX change this when we setup the TSS */
	/*ifrm.regs.esp = ctx->stack_ptr;*/		/* ... until then... */
	ifrm.esp = ctx->stack_ptr;			/* this will only be used when we switch to userspace */
	ifrm.ss = selector(SEGM_KDATA, 0);

	/* switch to the vm of the process */
	set_pgdir_addr(ctx->pgtbl_paddr);
	intr_ret(ifrm);
}

int get_current_pid(void)
{
	return cur_pid;
}

struct process *get_current_proc(void)
{
	return cur_pid ? &proc[cur_pid] : 0;
}

struct process *get_process(int pid)
{
	return &proc[pid];
}
