/* definitions that must be in-sync between kernel and user space */
#ifndef KERNEL_DEFS_H_
#define KERNEL_DEFS_H_

/* --- defines for sys/wait.h */
#if defined(KERNEL) || defined(KDEF_WAIT_H)
#define WNOHANG		1

#define WEXITSTATUS(s)	((s) & _WSTATUS_MASK)
#define WCOREDUMP(s)	((s) & _WCORE_BIT)

#define WIFEXITED(s)	(_WREASON(s) == _WREASON_EXITED)
#define WIFSIGNALED(s)	(_WREASON(s) == _WREASON_SIGNALED)

/* implementation details */
#define _WSTATUS_MASK		0xff

#define _WREASON_SHIFT		8
#define _WREASON_MASK		0xf00
#define _WREASON(s)			(((s) & _WREASON_MASK) >> _WREASON_SHIFT)

#define _WREASON_EXITED		1
#define _WREASON_SIGNALED	2

#define _WCORE_BIT			0x1000
#endif	/* sys/wait.h */



/* --- defines for errno.h */
#if defined(KERNEL) || defined(KDEF_ERRNO_H)
#define EAGAIN		1
#define EINVAL		2
#define ECHILD		3
#define EBUSY		4
#define ENOMEM		5
#define EIO			6

#define EBUG		127	/* error: feature not implemented yet */
#endif	/* errno.h */


/* --- defines for syscall.h */
#if defined(KERNEL) || defined(KDEF_SYSCALL_H)

#define SYSCALL_INT		0x80

#define SYS_HELLO		0
#define SYS_SLEEP		1
#define SYS_FORK		2
#define SYS_EXIT		3
#define SYS_WAITPID		4
#define SYS_GETPID		5
#define SYS_GETPPID		6
#define SYS_MOUNT		7
#define SYS_UMOUNT		8
#define SYS_OPEN		9
#define SYS_CLOSE		10
#define SYS_READ		11
#define SYS_WRITE		12
#define SYS_LSEEK		13

/* keep this one more than the last syscall */
#define NUM_SYSCALLS	14

#endif	/* syscall.h */

#endif	/* KERNEL_DEFS_H_ */
