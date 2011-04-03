#include <stdio.h>
#include <string.h>
#include "mem.h"
#include "panic.h"
#include "vm.h"

#define FREE		0
#define USED		1

#define BM_IDX(pg)	((pg) / 32)
#define BM_BIT(pg)	((pg) & 0x1f)

#define IS_FREE(pg) ((bitmap[BM_IDX(pg)] & (1 << BM_BIT(pg))) == 0)

static void mark_page(int pg, int free);
static void add_memory(uint32_t start, size_t size);

/* end of kernel image */
extern int _end;

/* A bitmap is used to track which physical memory pages are used or available
 * for allocation by alloc_phys_page.
 *
 * last_alloc_idx keeps track of the last 32bit element in the bitmap array
 * where a free page was found. It's guaranteed that all the elements before
 * this have no free pages, but it doesn't imply that there will be another
 * free page there. So it's used as a starting point for the search.
 */
static uint32_t *bitmap;
static int bmsize, last_alloc_idx;


void init_mem(struct mboot_info *mb)
{
	int i, num_pages, max_pg = 0;
	uint32_t used_end;

	num_pages = 0;
	last_alloc_idx = 0;

	/* the allocation bitmap starts right at the end of the ELF image */
	bitmap = (uint32_t*)&_end;

	/* start by marking all posible pages (2**20) as used. We do not "reserve"
	 * all this space. Pages beyond the end of the useful bitmap area
	 * ((char*)bitmap + bmsize), which will be determined after we traverse the
	 * memory map, are going to be marked as available for allocation.
	 */
	memset(bitmap, 0xff, 1024 * 1024 / 8);

	/* if the bootloader gave us an available memory map, traverse it and mark
	 * all the corresponding pages as free.
	 */
	if(mb->flags & MB_MMAP) {
		struct mboot_mmap *mem, *mmap_end;

		mem = mb->mmap;
		mmap_end = (struct mboot_mmap*)((char*)mb->mmap + mb->mmap_len);

		printf("memory map:\n");
		while(mem < mmap_end) {
			/* ignore memory ranges that start beyond the 4gb mark */
			if(mem->base_high == 0 && mem->base_low != 0xffffffff) {
				char *type;
				unsigned int end, rest = 0xffffffff - mem->base_low;

				/* make sure the length does not extend beyond 4gb */
				if(mem->length_high || mem->length_low > rest) {
					mem->length_low = rest;
				}
				end	= mem->base_low + mem->length_low;

				if(mem->type == MB_MEM_VALID) {
					type = "free:";
					add_memory(mem->base_low, mem->length_low);

					num_pages = ADDR_TO_PAGE(mem->base_low + mem->length_low);
					if(max_pg < num_pages) {
						max_pg = num_pages;
					}
				} else {
					type = "hole:";
				}

				printf("  %s %x - %x (%u bytes)\n", type, mem->base_low, end, mem->length_low);
			}
			mem = (struct mboot_mmap*)((char*)mem + mem->skip + sizeof mem->skip);
		}
	} else if(mb->flags & MB_MEM) {
		/* if we don't have a detailed memory map, just use the lower and upper
		 * memory block sizes to determine which pages should be available.
		 */
		add_memory(0, mb->mem_lower);
		add_memory(0x100000, mb->mem_upper * 1024);
		max_pg = mb->mem_upper / 4;

		printf("lower memory: %ukb, upper mem: %ukb\n", mb->mem_lower, mb->mem_upper);
	} else {
		/* I don't think this should ever happen with a multiboot-compliant boot loader */
		panic("didn't get any memory info from the boot loader, I give up\n");
	}

	bmsize = max_pg / 8;	/* size of the useful bitmap in bytes */

	/* mark all the used pages as ... well ... used */
	used_end = ((uint32_t)bitmap + bmsize - 1);

	printf("marking pages up to %x ", used_end);
	used_end = ADDR_TO_PAGE(used_end);
	printf("(page: %d) inclusive as used\n", used_end);

	for(i=0; i<=used_end; i++) {
		mark_page(i, USED);
	}
}

/* alloc_phys_page finds the first available page of physical memory,
 * marks it as used in the bitmap, and returns its address. If there's
 * no unused physical page, 0 is returned.
 */
uint32_t alloc_phys_page(void)
{
	int i, idx, max;

	idx = last_alloc_idx;
	max = bmsize / 4;

	while(idx <= max) {
		/* if at least one bit is 0 then we have at least
		 * one free page. find it and allocate it.
		 */
		if(bitmap[idx] != 0xffffffff) {
			for(i=0; i<32; i++) {
				int pg = idx * 32 + i;

				if(IS_FREE(pg)) {
					mark_page(pg, USED);

					last_alloc_idx = idx;

					printf("alloc_phys_page() -> %x (page: %d)\n", PAGE_TO_ADDR(pg), pg);
					return PAGE_TO_ADDR(pg);
				}
			}
			panic("can't happen: alloc_phys_page (mem.c)\n");
		}
		idx++;
	}

	return 0;
}

/* free_phys_page marks the physical page which corresponds to the specified
 * address as free in the allocation bitmap.
 *
 * CAUTION: no checks are done that this page should actually be freed or not.
 * If you call free_phys_page with the address of some part of memory that was
 * originally reserved due to it being in a memory hole or part of the kernel
 * image or whatever, it will be subsequently allocatable by alloc_phys_page.
 */
void free_phys_page(uint32_t addr)
{
	int pg = ADDR_TO_PAGE(addr);
	int bmidx = BM_IDX(pg);

	if(!IS_FREE(pg)) {
		panic("free_phys_page(%d): I thought that was already free!\n", pg);
	}

	mark_page(pg, FREE);
	if(bmidx < last_alloc_idx) {
		last_alloc_idx = bmidx;
	}
}

/* this is only ever used by the VM init code to find out what the extends of
 * the kernel image are, in order to map them 1-1 before enabling paging.
 */
void get_kernel_mem_range(uint32_t *start, uint32_t *end)
{
	if(start) {
		*start = 0x100000;
	}
	if(end) {
		uint32_t e = (uint32_t)bitmap + bmsize;

		if(e & PGOFFS_MASK) {
			*end = (e + 4096) & PGOFFS_MASK;
		} else {
			*end = e;
		}
	}
}

/* adds a range of physical memory to the available pool. used during init_mem
 * when traversing the memory map.
 */
static void add_memory(uint32_t start, size_t sz)
{
	int i, szpg, pg;

	szpg = ADDR_TO_PAGE(sz);
	pg = ADDR_TO_PAGE(start);

	for(i=0; i<szpg; i++) {
		mark_page(pg++, FREE);
	}
}

/* maps a page as used or free in the allocation bitmap */
static void mark_page(int pg, int used)
{
	int idx = BM_IDX(pg);
	int bit = BM_BIT(pg);

	if(used) {
		bitmap[idx] |= 1 << bit;
	} else {
		bitmap[idx] &= ~(1 << bit);
	}
}

