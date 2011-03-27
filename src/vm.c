#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "vm.h"
#include <stdio.h>
#include "intr.h"
#include "mem.h"
#include "panic.h"


/* defined in vm-asm.S */
void enable_paging(void);
void set_pgdir_addr(uint32_t addr);
uint32_t get_fault_addr(void);

static void pgfault(int inum, uint32_t err);

/* page directory */
static uint32_t *pgdir;

#define KMEM_START		0xc0000000
#define IDMAP_START		0xa0000

#define ATTR_PGDIR_MASK	0x3f
#define ATTR_PGTBL_MASK	0x1ff
#define ADDR_PGENT_MASK	0xfffff000

#define PAGEFAULT		14

void init_vm(struct mboot_info *mb)
{
	init_mem(mb);

	pgdir = (uint32_t*)alloc_phys_page();
	memset(pgdir, 0, sizeof pgdir);

	/* map the video memory and kernel code 1-1 */
	map_mem_range(IDMAP_START, MAX_BRK - IDMAP_START, IDMAP_START, 0);

	interrupt(PAGEFAULT, pgfault);

	set_pgdir_addr((int32_t)pgdir);
	enable_paging();
}

void map_page(int vpage, int ppage, unsigned int attr)
{
	uint32_t *pgtbl;
	int diridx = PAGE_TO_PGTBL(vpage);
	int pgidx = PAGE_TO_PGTBL_PG(vpage);

	if(!(pgdir[diridx] & PG_PRESENT)) {
		uint32_t addr = alloc_phys_page();
		pgtbl = (uint32_t*)addr;
		memset(pgtbl, 0, PGSIZE);

		pgdir[diridx] = addr | (attr & ATTR_PGDIR_MASK) | PG_PRESENT;
	} else {
		pgtbl = (uint32_t*)(pgdir[diridx] & ADDR_PGENT_MASK);
	}

	pgtbl[pgidx] = PAGE_TO_ADDR(ppage) | (attr & ATTR_PGTBL_MASK) | PG_PRESENT;
}

void unmap_page(int vpage)
{
	uint32_t *pgtbl;
	int diridx = PAGE_TO_PGTBL(vpage);
	int pgidx = PAGE_TO_PGTBL_PG(vpage);

	if(!(pgdir[diridx] & PG_PRESENT)) {
		goto err;
	}
	pgtbl = (uint32_t*)(pgdir[diridx] & ADDR_PGENT_MASK);

	if(!(pgtbl[pgidx] & PG_PRESENT)) {
		goto err;
	}
	pgtbl[pgidx] = 0;

	return;
err:
	printf("unmap_page(%d): page already not mapped\n", vpage);
}

void map_page_range(int vpg_start, int pgcount, int ppg_start, unsigned int attr)
{
	int i;

	for(i=0; i<pgcount; i++) {
		map_page(vpg_start + i, ppg_start + i, attr);
	}
}

void map_mem_range(uint32_t vaddr, size_t sz, uint32_t paddr, unsigned int attr)
{
	int vpg_start, ppg_start, num_pages;

	if(!sz) return;

	if(ADDR_TO_PGOFFS(paddr)) {
		panic("map_mem_range called with unaligned physical address: %x\n", paddr);
	}

	vpg_start = ADDR_TO_PAGE(vaddr);
	ppg_start = ADDR_TO_PAGE(paddr);
	num_pages = ADDR_TO_PAGE(sz) + 1;

	map_page_range(vpg_start, num_pages, ppg_start, attr);
}


/*	if(mb->flags & MB_MMAP) {
		struct mboot_mmap *mem, *mmap_end;

		mem = mb->mmap;
		mmap_end = (struct mboot_mmap*)((char*)mb->mmap + mb->mmap_len);

		printf("memory map:\n");
		while(mem < mmap_end) {
			unsigned int end = mem->base_low + mem->length_low;
			char *type = mem->type == MB_MEM_VALID ? "free:" : "hole:";

			printf("  %s %x - %x (%u bytes)\n", type, mem->base_low, end, mem->length_low);
			mem = (struct mboot_mmap*)((char*)mem + mem->skip + sizeof mem->skip);
		}
	}

	if(mb->flags & MB_MEM) {
		printf("lower memory: %ukb, upper mem: %ukb\n", mb->mem_lower, mb->mem_upper);
	}
*/

uint32_t virt_to_phys(uint32_t vaddr)
{
	uint32_t pgaddr, *pgtbl;
	int diridx = ADDR_TO_PGTBL(vaddr);
	int pgidx = ADDR_TO_PGTBL_PG(vaddr);

	if(!(pgdir[diridx] & PG_PRESENT)) {
		panic("virt_to_phys(%x): page table %d not present\n", vaddr, diridx);
	}
	pgtbl = (uint32_t*)(pgdir[diridx] & PGENT_ADDR_MASK);

	if(!(pgtbl[pgidx] & PG_PRESENT)) {
		panic("virt_to_phys(%x): page %d not present\n", vaddr, ADDR_TO_PAGE(vaddr));
	}
	pgaddr = pgtbl[pgidx] & PGENT_ADDR_MASK;

	return pgaddr | ADDR_TO_PGOFFS(vaddr);
}

static void pgfault(int inum, uint32_t err)
{
	printf("~~~~ PAGE FAULT ~~~~\n");

	printf("fault address: %x\n", get_fault_addr());

	if(err & PG_PRESENT) {
		if(err & 8) {
			printf("reserved bit set in some paging structure\n");
		} else {
			printf("%s protection violation ", (err & PG_WRITABLE) ? "write" : "read");
			printf("in %s mode\n", err & PG_USER ? "user" : "kernel");
		}
	} else {
		printf("page not present\n");
	}
}
