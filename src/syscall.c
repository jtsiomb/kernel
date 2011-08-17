#include <stdio.h>
#include "syscall.h"
#include "intr.h"
#include "proc.h"
#include "sched.h"
#include "timer.h"

static int (*sys_func[NUM_SYSCALLS])();

static void syscall(int inum);

static int sys_hello(void);
static int sys_sleep(int sec);
static int sys_fork(void);
static int sys_getpid(void);

void init_syscall(void)
{
	sys_func[SYS_HELLO] = sys_hello;
	sys_func[SYS_SLEEP] = sys_sleep;
	sys_func[SYS_FORK] = sys_fork;
	sys_func[SYS_GETPID] = sys_getpid;

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

static int sys_hello(void)
{
	printf("process %d says hello!\n", get_current_pid());
	return 0;
}

static int sys_sleep(int sec)
{
	printf("process %d will sleep for %d seconds\n", get_current_pid(), sec);
	sleep(sec * 1000); /* timer.c */
	return 0;
}

static int sys_fork(void)
{
	printf("process %d is forking\n", get_current_pid());
	return fork(); /* proc.c */
}

static int sys_getpid(void)
{
	int pid = get_current_pid();
	printf("process %d getpid\n", pid);
	return pid;
}
