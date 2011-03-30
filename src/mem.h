#ifndef MEM_H_
#define MEM_H_

#include "mboot.h"

void init_mem(struct mboot_info *mb);

uint32_t alloc_phys_page(void);
void free_phys_page(uint32_t addr);

void get_kernel_mem_range(uint32_t *start, uint32_t *end);

#endif	/* MEM_H_ */
