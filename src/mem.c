#include <stdio.h>
#include "mem.h"
#include "panic.h"
#include "vm.h"

/* end of kernel image */
extern int _end;

static uint32_t brk;

void init_mem(struct mboot_info *mb)
{
	/* start the physical allocated break at the end of
	 * the kernel image
	 */
	brk = (uint32_t)&_end;
}

uint32_t alloc_phys_page(void)
{
	uint32_t addr, dbg_prev_brk;

	if(ADDR_TO_PGOFFS(brk)) {
		/* brk is not aligned, find the next page-aligned address */
		addr = (brk + PGSIZE) & ~PGOFFS_MASK;
	} else {
		/* brk is aligned, so we can use that address directly */
		addr = brk;
	}

	if(addr >= MAX_BRK) {
		panic("alloc_phys_page() out of early alloc space");
	}

	dbg_prev_brk = brk;
	brk = addr + PGSIZE;	/* move the break to the end of the page */

	printf("DBG: alloc_phys_page(): %x  (brk %x -> %x)\n", addr, dbg_prev_brk, brk);
	return addr;
}
