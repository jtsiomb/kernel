#ifndef SCHED_H_
#define SCHED_H_

#include "proc.h"

void schedule(void);

int add_proc(int pid, enum proc_state state);
int block_proc(int pid);
int unblock_proc(int pid);

#endif	/* SCHED_H_ */
