#include <stdio.h>
#include "syscall.h"
#include "intr.h"
#include "proc.h"
#include "sched.h"
#include "timer.h"

static int (*sys_func[NUM_SYSCALLS])();

static void syscall(int inum, struct intr_frame *frm);

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

static void syscall(int inum, struct intr_frame *frm)
{
	int idx = frm->regs.eax;

	if(idx < 0 || idx >= NUM_SYSCALLS) {
		printf("invalid syscall: %d\n", idx);
		return;
	}

	frm->regs.eax = sys_func[idx](frm->regs.ebx, frm->regs.ecx, frm->regs.edx, frm->regs.esi, frm->regs.edi);
	schedule();
}

static int sys_exit(int status)
{
	return -1;	/* not implemented yet */
}

static int sys_hello(void)
{
	/*printf("process %d says hello!\n", get_current_pid());*/
	return 0;
}

static int sys_sleep(int sec)
{
	int pid = get_current_pid();
	/*printf("process %d will sleep for %d sec\n", pid, sec);*/
	start_timer(sec * 1000, (timer_func_t)unblock_proc, (void*)pid);
	block_proc(pid);
	return 0;
}
