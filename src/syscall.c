#include <stdio.h>
#include "syscall.h"
#include "intr.h"
#include "proc.h"
#include "sched.h"
#include "timer.h"
#include "fs.h"

static int (*sys_func[NUM_SYSCALLS])();

static void syscall(int inum);

static int sys_hello(void);

void init_syscall(void)
{
	sys_func[SYS_HELLO] = sys_hello;
	sys_func[SYS_SLEEP] = sys_sleep;		/* timer.c */
	sys_func[SYS_FORK] = sys_fork;			/* proc.c */
	sys_func[SYS_EXIT] = sys_exit;			/* proc.c */
	sys_func[SYS_WAITPID] = sys_waitpid;	/* proc.c */
	sys_func[SYS_GETPID] = sys_getpid;		/* proc.c */
	sys_func[SYS_GETPPID] = sys_getppid;	/* proc.c */

	sys_func[SYS_MOUNT] = sys_mount;		/* fs.c */
	sys_func[SYS_UMOUNT] = sys_umount;		/* fs.c */
	sys_func[SYS_OPEN] = sys_open;			/* fs.c */
	sys_func[SYS_CLOSE] = sys_close;		/* fs.c */
	sys_func[SYS_READ] = sys_read;			/* fs.c */
	sys_func[SYS_WRITE] = sys_write;		/* fs.c */
	sys_func[SYS_LSEEK] = sys_lseek;		/* fs.c */

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

	/* we don't necessarily want to return to the same process
	 * might have blocked or exited or whatever, so call schedule
	 * to decide what's going to run next.
	 */
	schedule();
}

static int sys_hello(void)
{
	printf("process %d says hello!\n", get_current_pid());
	return 0;
}
