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

static uint32_t *bitmap;
static int bmsize, last_alloc_idx;

void init_mem(struct mboot_info *mb)
{
	int i, num_pages, max_pg = 0;
	uint32_t used_end;

	num_pages = 0;
	last_alloc_idx = 0;

	bitmap = (uint32_t*)&_end;

	/* start by marking all posible pages as used */
	memset(bitmap, 0xff, 1024 * 1024 / 8);

	/* build the memory map */
	if(mb->flags & MB_MMAP) {
		struct mboot_mmap *mem, *mmap_end;

		mem = mb->mmap;
		mmap_end = (struct mboot_mmap*)((char*)mb->mmap + mb->mmap_len);

		printf("memory map:\n");
		while(mem < mmap_end) {
			char *type;
			unsigned int end = mem->base_low + mem->length_low;

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
			mem = (struct mboot_mmap*)((char*)mem + mem->skip + sizeof mem->skip);
		}
	} else if(mb->flags & MB_MEM) {
		add_memory(0, mb->mem_lower);
		add_memory(0x100000, mb->mem_upper * 1024);
		max_pg = mb->mem_upper / 4;

		printf("lower memory: %ukb, upper mem: %ukb\n", mb->mem_lower, mb->mem_upper);
	} else {
		panic("didn't get any memory info from the boot loader, I give up\n");
	}

	bmsize = max_pg / 8;	/* size of the bitmap in bytes */

	/* mark all the used pages as ... well ... used */
	used_end = ((uint32_t)bitmap + bmsize - 1);

	printf("marking pages up to %x ", used_end);
	used_end = ADDR_TO_PAGE(used_end);
	printf("(page: %d) inclusive as used\n", used_end);

	for(i=0; i<=used_end; i++) {
		mark_page(i, USED);
	}

	/*for(i=0; i<bmsize / 4; i++) {
		printf("%3d [%x]\n", i, bitmap[i]);
		asm("hlt");
	}
	putchar('\n');*/
}

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

	panic("alloc_phys_page(): out of memory\n");
	return 0;
}

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

static void add_memory(uint32_t start, size_t sz)
{
	int i, szpg, pg;

	szpg = ADDR_TO_PAGE(sz);
	pg = ADDR_TO_PAGE(start);

	for(i=0; i<szpg; i++) {
		mark_page(pg++, FREE);
	}
}

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

