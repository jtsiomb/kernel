#ifndef SYSCALL_H_
#define SYSCALL_H_

#define SYSCALL_INT		0x80

/* when we get rid of test_proc.S we'll turn this into an enum */
#define SYS_EXIT		0
#define SYS_HELLO		1
#define SYS_SLEEP		2

#define NUM_SYSCALLS	3

#ifndef ASM
void init_syscall(void);
#endif

#endif	/* SYSCALL_H_ */
