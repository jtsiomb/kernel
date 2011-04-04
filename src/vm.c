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
void disable_paging(void);
int get_paging_status(void);
void set_pgdir_addr(uint32_t addr);
void flush_tlb(void);
void flush_tlb_addr(uint32_t addr);
#define flush_tlb_page(p)	flush_tlb_addr(PAGE_TO_ADDR(p))
uint32_t get_fault_addr(void);

static void coalesce(struct page_range *low, struct page_range *mid, struct page_range *high);
static void pgfault(int inum, uint32_t err);
static struct page_range *alloc_node(void);
static void free_node(struct page_range *node);

/* page directory */
static uint32_t *pgdir;

/* 2 lists of free ranges, for kernel memory and user memory */
static struct page_range *pglist[2];
/* list of free page_range structures to be used in the lists */
static struct page_range *node_pool;
/* the first page range for the whole kernel address space, to get things started */
static struct page_range first_node;


void init_vm(struct mboot_info *mb)
{
	uint32_t idmap_end;

	/* initialize the physical memory map and allocator */
	init_mem(mb);

	/* setup the page tables */
	pgdir = (uint32_t*)alloc_phys_page();
	memset(pgdir, 0, PGSIZE);
	set_pgdir_addr((int32_t)pgdir);

	/* map the video memory and kernel code 1-1 */
	get_kernel_mem_range(0, &idmap_end);
	map_mem_range(IDMAP_START, idmap_end - IDMAP_START, IDMAP_START, 0);

	/* set the page fault handler */
	interrupt(PAGEFAULT, pgfault);

	/* we can enable paging now */
	enable_paging();

	/* initialize the virtual page allocator */
	node_pool = 0;

	first_node.start = ADDR_TO_PAGE(KMEM_START);
	first_node.end = PAGE_COUNT;
	first_node.next = 0;
	pglist[MEM_KERNEL] = &first_node;

	pglist[MEM_USER] = alloc_node();
	pglist[MEM_USER]->start = 0;
	pglist[MEM_USER]->end = ADDR_TO_PAGE(KMEM_START);
	pglist[MEM_USER]->next = 0;
}

/* if ppage == -1 we allocate a physical page by calling alloc_phys_page */
int map_page(int vpage, int ppage, unsigned int attr)
{
	uint32_t *pgtbl;
	int diridx, pgidx, pgon;

	pgon = get_paging_status();
	disable_paging();

	if(ppage < 0) {
		uint32_t addr = alloc_phys_page();
		if(!addr) {
			return -1;
		}
		ppage = ADDR_TO_PAGE(addr);
	}

	diridx = PAGE_TO_PGTBL(vpage);
	pgidx = PAGE_TO_PGTBL_PG(vpage);

	if(!(pgdir[diridx] & PG_PRESENT)) {
		uint32_t addr = alloc_phys_page();
		pgtbl = (uint32_t*)addr;
		memset(pgtbl, 0, PGSIZE);

		pgdir[diridx] = addr | (attr & ATTR_PGDIR_MASK) | PG_PRESENT;
	} else {
		pgtbl = (uint32_t*)(pgdir[diridx] & ADDR_PGENT_MASK);
	}

	pgtbl[pgidx] = PAGE_TO_ADDR(ppage) | (attr & ATTR_PGTBL_MASK) | PG_PRESENT;
	flush_tlb_page(vpage);

	if(pgon) {
		enable_paging();
	}
	return 0;
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
	flush_tlb_page(vpage);

	return;
err:
	printf("unmap_page(%d): page already not mapped\n", vpage);
}

/* if ppg_start is -1, we allocate physical pages to map with alloc_phys_page() */
int map_page_range(int vpg_start, int pgcount, int ppg_start, unsigned int attr)
{
	int i, phys_pg;
	uint32_t paddr;

	for(i=0; i<pgcount; i++) {
		if(ppg_start < 0) {
			if(!(paddr = alloc_phys_page())) {
				return -1;
			}
			phys_pg = ADDR_TO_PAGE(paddr);
		} else {
			phys_pg = ppg_start + i;
		}

		map_page(vpg_start + i, phys_pg, attr);
	}
	return 0;
}

/* if paddr is 0, we allocate physical pages with alloc_phys_page() */
int map_mem_range(uint32_t vaddr, size_t sz, uint32_t paddr, unsigned int attr)
{
	int vpg_start, ppg_start, num_pages;

	if(!sz) return -1;

	if(ADDR_TO_PGOFFS(paddr)) {
		panic("map_mem_range called with unaligned physical address: %x\n", paddr);
	}

	vpg_start = ADDR_TO_PAGE(vaddr);
	ppg_start = paddr > 0 ? ADDR_TO_PAGE(paddr) : -1;
	num_pages = ADDR_TO_PAGE(sz) + 1;

	return map_page_range(vpg_start, num_pages, ppg_start, attr);
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
		/* allocate physical storage and map */
		if(map_page_range(ret, num, -1, 0) == -1) {
			ret = -1;
		}
	}

	return ret;
}

void pgfree(int start, int num)
{
	int area, end;
	struct page_range *node, *new, *prev, *next;

	if(!(new = alloc_node())) {
		panic("pgfree: can't allocate new page_range node to add the freed pages\n");
	}
	new->start = start;
	end = new->end = start + num;

	area = PAGE_TO_ADDR(start) >= KMEM_START ? MEM_KERNEL : MEM_USER;

	if(!pglist[area] || pglist[area]->start > start) {
		next = new->next = pglist[area];
		pglist[area] = new;
		prev = 0;

	} else {

		prev = 0;
		node = pglist[area];
		next = node ? node->next : 0;

		while(node) {
			if(!next || next->start > start) {
				/* place here, after node */
				new->next = next;
				node->next = new;
				prev = node;	/* needed by coalesce after the loop */
				break;
			}

			prev = node;
			node = next;
			next = node ? node->next : 0;
		}
	}

	coalesce(prev, new, next);
}

static void coalesce(struct page_range *low, struct page_range *mid, struct page_range *high)
{
	if(high) {
		if(mid->end == high->start) {
			mid->end = high->end;
			mid->next = high->next;
			free_node(high);
		}
	}

	if(low) {
		if(low->end == mid->start) {
			low->end += mid->end;
			low->next = mid->next;
			free_node(mid);
		}
	}
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
#define NODES_IN_PAGE	(PGSIZE / sizeof(struct page_range))

static struct page_range *alloc_node(void)
{
	struct page_range *node;
	int pg, i;

	if(node_pool) {
		node = node_pool;
		node_pool = node_pool->next;
		printf("alloc_node -> %x\n", (unsigned int)node);
		return node;
	}

	/* no node structures in the pool, we need to allocate a new page,
	 * split it up into node structures, add them in the pool, and
	 * allocate one of them.
	 */
	if(!(pg = pgalloc(1, MEM_KERNEL))) {
		panic("ran out of physical memory while allocating VM range structures\n");
	}
	node_pool = (struct page_range*)PAGE_TO_ADDR(pg);

	/* link them up, skip the first as we'll just allocate it anyway */
	for(i=2; i<NODES_IN_PAGE; i++) {
		node_pool[i - 1].next = node_pool + i;
	}
	node_pool[NODES_IN_PAGE - 1].next = 0;

	/* grab the first and return it */
	node = node_pool++;
	printf("alloc_node -> %x\n", (unsigned int)node);
	return node;
}

static void free_node(struct page_range *node)
{
	node->next = node_pool;
	node_pool = node;
	printf("free_node\n");
}


void dbg_print_vm(int area)
{
	struct page_range *node = pglist[area];
	int last = area == MEM_USER ? 0 : ADDR_TO_PAGE(KMEM_START);

	printf("%s vm space\n", area == MEM_USER ? "user" : "kernel");

	while(node) {
		if(node->start > last) {
			printf("  vm-used: %x -> %x\n", PAGE_TO_ADDR(last), PAGE_TO_ADDR(node->start));
		}

		printf("  vm-free: %x -> ", PAGE_TO_ADDR(node->start));
		if(node->end >= PAGE_COUNT) {
			printf("END\n");
		} else {
			printf("%x\n", PAGE_TO_ADDR(node->end));
		}

		last = node->end;
		node = node->next;
	}
}
