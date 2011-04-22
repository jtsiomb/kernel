#ifndef PROC_H_
#define PROC_H_

#include "tss.h"

struct process {
	int id;

	struct task_state tss;

	struct process *next;
};

#endif	/* PROC_H_ */
