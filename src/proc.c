#include "proc.h"
#include "tss.h"

static struct process proc[MAX_PROC];
static int cur_pid;

void init_proc(void)
{
	cur_pid = -1;

	/* prepare the first process */

	/* create the virtual address space for this process */

	/* allocate a chunk of memory for the process image
	 * and copy the code of test_proc there.
	 * (should be mapped at a fixed address)
	 */

	/* fill in the proc[0].ctx with the appropriate process stack
	 * and instruction pointers
	 */

	/* switch to it by calling a function that takes the context
	 * of the current process, plugs the values into the interrupt
	 * stack, and calls iret.
	 * (should also set ss0/sp0 in TSS before returning)
	 */
}

void save_context(struct intr_frame *ifrm)
{
	proc[cur_pid].ctx->regs = ifrm->regs;
	proc[cur_pid].ctx->instr_ptr = ifrm->eip;
	proc[cur_pid].ctx->stack_ptr = ifrm->esp;
	proc[cur_pid].ctx->flags = ifrm->eflags;
}
