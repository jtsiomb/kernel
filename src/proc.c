#include <string.h>
#include "proc.h"
#include "tss.h"
#include "vm.h"
#include "segm.h"
#include "intr.h"
#include "panic.h"


/* defined in test_proc.S */
void test_proc(void);
void test_proc_end(void);

static struct process proc[MAX_PROC];
static int cur_pid;

void init_proc(void)
{
	int proc_size_pg, img_start_pg, stack_pg;
	void *img_start;
	cur_pid = -1;

	/* prepare the first process */

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

	/* instruction pointer at the beginning of the process image */
	proc[0].ctx.instr_ptr = (uint32_t)img_start;

	/* allocate the first page of the process stack */
	stack_pg = ADDR_TO_PAGE(KMEM_START) - 1;
	if(pgalloc_vrange(stack_pg, 1) == -1) {
		panic("failed to allocate user stack page\n");
	}
	proc[0].ctx.stack_ptr = PAGE_TO_ADDR(stack_pg) + PGSIZE;

	/* create the virtual address space for this process */
	proc[0].ctx.pgtbl_paddr = clone_vm();

	/* we don't need the image and the stack in this address space */
	unmap_page_range(img_start_pg, proc_size_pg);
	pgfree(img_start_pg, proc_size_pg);

	unmap_page(stack_pg);
	pgfree(stack_pg, 1);


	/* switch to it by calling a function that takes the context
	 * of the current process, plugs the values into the interrupt
	 * stack, and calls iret.
	 * (should also set ss0/sp0 in TSS before returning)
	 */
	context_switch(0);
	/* XXX this will never return */
}


void context_switch(int pid)
{
	struct intr_frame ifrm;
	struct context *ctx = &proc[pid].ctx;

	cur_pid = pid;

	ifrm.inum = ifrm.err = 0;

	ifrm.regs = ctx->regs;
	ifrm.eflags = ctx->flags;

	ifrm.eip = ctx->instr_ptr;
	ifrm.cs = SEGM_KCODE;	/* XXX change this when we setup the TSS */
	ifrm.esp = ctx->stack_ptr;
	ifrm.ss = SEGM_KDATA;	/* XXX */

	/* switch to the vm of the process */
	set_pgdir_addr(ctx->pgtbl_paddr);

	intr_ret(ifrm);
}
