#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "vm.h"
#include <stdio.h>
#include "intr.h"
#include "mem.h"
#include "panic.h"


#define KMEM_START		0xc0000000
#define IDMAP_START		0xa0000

#define ATTR_PGDIR_MASK	0x3f
#define ATTR_PGTBL_MASK	0x1ff
#define ADDR_PGENT_MASK	0xfffff000

#define PAGEFAULT		14


struct page_range {
	int start, end;
	struct page_range *next;
};

/* defined in vm-asm.S */
void enable_paging(void);
void set_pgdir_addr(uint32_t addr);
uint32_t get_fault_addr(void);

static void pgfault(int inum, uint32_t err);
static struct page_range *alloc_node(void);
static void free_node(struct page_range *node);

/* page directory */
static uint32_t *pgdir;

/* 2 lists of free ranges, for kernel memory and user memory */
static struct page_range *pglist[2];
/* list of free page_range structures to be used in the lists */
static struct page_range *node_pool;


void init_vm(struct mboot_info *mb)
{
	uint32_t idmap_end;

	init_mem(mb);

	pgdir = (uint32_t*)alloc_phys_page();
	memset(pgdir, 0, sizeof pgdir);

	/* map the video memory and kernel code 1-1 */
	get_kernel_mem_range(0, &idmap_end);
	map_mem_range(IDMAP_START, idmap_end - IDMAP_START, IDMAP_START, 0);

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

/* if ppg_start is -1, we allocate physical pages to map with alloc_phys_page() */
void map_page_range(int vpg_start, int pgcount, int ppg_start, unsigned int attr)
{
	int i;

	for(i=0; i<pgcount; i++) {
		uint32_t paddr = ppg_start == -1 ? alloc_phys_page() : ppg_start + i;

		map_page(vpg_start + i, paddr, attr);
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

/* allocate a contiguous block of virtual memory pages along with
 * backing physical memory for them, and update the page table.
 */
int pgalloc(int num, int area)
{
	int ret = -1;
	struct page_range *node, *prev, dummy;

	dummy.next = pglist[area];
	node = pglist[area];
	prev = &dummy;

	while(node) {
		if(node->end - node->start >= num) {
			ret = node->start;
			node->start += num;

			if(node->start == node->end) {
				prev->next = node->next;
				node->next = 0;

				if(node == pglist[area]) {
					pglist[area] = 0;
				}
				free_node(node);
			}
			break;
		}

		prev = node;
		node = node->next;
	}

	if(ret >= 0) {
		/* allocate physical storage and map them */
		map_page_range(ret, num, -1, 0);
	}

	return ret;
}

void pgfree(int start, int num)
{
	/* TODO */
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

	panic("unhandled page fault\n");
}

/* --- page range list node management --- */
static struct page_range *alloc_node(void)
{
	struct page_range *node;
	uint32_t paddr;

	if(node_pool) {
		node = node_pool;
		node_pool = node_pool->next;
		return node;
	}

	/* no node structures in the pool, we need to allocate and map
	 * a page, split it up into node structures, add them in the pool
	 * and allocate one of them.
	 */
	if(!(paddr = alloc_phys_page())) {
		panic("ran out of physical memory while allocating VM range structures\n");
	}

	/* TODO cont. */
	return 0;
}

static void free_node(struct page_range *node)
{
	node->next = node_pool;
	node_pool = node;
}
