#include <stdio.h>
#include "syscall.h"
#include "intr.h"
#include "proc.h"
#include "sched.h"
#include "timer.h"

static int (*sys_func[NUM_SYSCALLS])();

static void syscall(int inum);

static int sys_exit(int status);
static int sys_hello(void);
static int sys_sleep(int sec);

void init_syscall(void)
{
	sys_func[SYS_EXIT] = sys_exit;
	sys_func[SYS_HELLO] = sys_hello;
	sys_func[SYS_SLEEP] = sys_sleep;

	interrupt(SYSCALL_INT, syscall);
}

static void syscall(int inum)
{
	struct intr_frame *frm;
	int idx;

	frm = get_intr_frame();
	idx = frm->regs.eax;

	if(idx < 0 || idx >= NUM_SYSCALLS) {
		printf("invalid syscall: %d\n", idx);
		return;
	}

	/* the return value goes into the interrupt frame copy of the user's eax
	 * so that it'll be restored into eax before returning to userland.
	 */
	frm->regs.eax = sys_func[idx](frm->regs.ebx, frm->regs.ecx, frm->regs.edx, frm->regs.esi, frm->regs.edi);
}

static int sys_exit(int status)
{
	printf("SYSCALL: exit\n");
	return -1;	/* not implemented yet */
}

static int sys_hello(void)
{
	printf("process %d says hello!\n", get_current_pid());
	return 0;
}

static int sys_sleep(int sec)
{
	printf("SYSCALL: sleep\n");
	return -1;
}
