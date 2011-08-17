#ifndef SYSCALL_H_
#define SYSCALL_H_

#define SYSCALL_INT		0x80

/* when we get rid of test_proc.S we'll turn this into an enum */
#define SYS_HELLO		0
#define SYS_SLEEP		1
#define SYS_FORK		2
#define SYS_GETPID		3

#define NUM_SYSCALLS	4

#ifndef ASM
void init_syscall(void);
#endif

#endif	/* SYSCALL_H_ */
