#include "proc.h"
#include "tss.h"
#include "vm.h"

static struct process proc[MAX_PROC];
static int cur_pid;

void init_proc(void)
{
	int proc_size_pg, img_start_pg;
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

	/* create the virtual address space for this process */
	proc[0].ctx.pgtbl_paddr = clone_vmem();

	/* we don't need the image in this address space */
	unmap_pa
	pgfree(img_start_pg, proc_size_pg);


	/* fill in the proc[0].ctx with the appropriate process stack
	 * and instruction pointers
	 */

	/* switch to it by calling a function that takes the context
	 * of the current process, plugs the values into the interrupt
	 * stack, and calls iret.
	 * (should also set ss0/sp0 in TSS before returning)
	 */
}

/*
void save_context(struct intr_frame *ifrm)
{
	proc[cur_pid].ctx->regs = ifrm->regs;
	proc[cur_pid].ctx->instr_ptr = ifrm->eip;
	proc[cur_pid].ctx->stack_ptr = ifrm->esp;
	proc[cur_pid].ctx->flags = ifrm->eflags;
}*/
