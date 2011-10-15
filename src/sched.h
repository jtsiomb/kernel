#ifndef SCHED_H_
#define SCHED_H_

#include "proc.h"

void schedule(void);

void add_proc(int pid);
void remove_proc(int pid);

void wait(void *wait_addr);
void wakeup(void *wait_addr);

#endif	/* SCHED_H_ */
