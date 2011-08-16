#ifndef SYSCALL_H_
#define SYSCALL_H_

#define SYSCALL_INT		0x80

/* when we get rid of test_proc.S we'll turn this into an enum */
#define SYS_EXIT		0
#define SYS_HELLO		1
#define SYS_SLEEP		2
#define SYS_FORK		3
#define SYS_GETPID		4

#define NUM_SYSCALLS	5

#ifndef ASM
void init_syscall(void);
#endif

#endif	/* SYSCALL_H_ */
