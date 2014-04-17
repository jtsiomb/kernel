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
#define EFOO		1 /* I just like to return -1 some times :) */

#define EAGAIN			2
#define EINVAL			3
#define ECHILD			4
#define EBUSY			5
#define ENOMEM			6
#define EIO				7
#define ENOENT			8
#define ENAMETOOLONG	9
#define ENOSPC			10
#define EPERM			11
#define ENOTDIR			12

#define EBUG		127	/* for missing features and known bugs */
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

/* --- defines for sys/stat.h */
#if defined(KERNEL) || defined(STAT_H)

#define S_IFMT		0170000	/* bit mask for the file type bit fields */
#define S_IFSOCK	0140000	/* socket */
#define S_IFLNK		0120000	/* symbolic link */
#define S_IFREG		0100000	/* regular file */
#define S_IFBLK		0060000	/* block device */
#define S_IFDIR		0040000	/* directory */
#define S_IFCHR		0020000	/* character device */
#define S_IFIFO		0010000	/* FIFO */

#define S_ISUID		0004000	/* set UID bit */
#define S_ISGID		0002000	/* set-group-ID bit (see below) */
#define S_ISVTX		0001000	/* sticky bit (see below) */

#define S_IRWXU		00700	/* mask for file owner permissions */
#define S_IRUSR		00400	/* owner has read permission */
#define S_IWUSR		00200	/* owner has write permission */
#define S_IXUSR		00100	/* owner has execute permission */
#define S_IRWXG		00070	/* mask for group permissions */
#define S_IRGRP		00040	/* group has read permission */
#define S_IWGRP		00020	/* group has write permission */
#define S_IXGRP		00010	/* group has execute permission */
#define S_IRWXO		00007	/* mask for permissions for others (not in group) */
#define S_IROTH		00004	/* others have read permission */
#define S_IWOTH		00002	/* others have write permission */
#define S_IXOTH		00001	/* others have execute permission */

#endif	/* sys/stat.h */


#endif	/* KERNEL_DEFS_H_ */
