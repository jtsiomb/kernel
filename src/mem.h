#ifndef MEM_H_
#define MEM_H_

#include "mboot.h"

/* maximum break for the early allocator */
#define MAX_BRK		0x400000

void init_mem(struct mboot_info *mb);
uint32_t alloc_phys_page(void);

#endif	/* MEM_H_ */
