#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "config.h"
#include "vm.h"
#include "intr.h"
#include "mem.h"
#include "panic.h"
#include "proc.h"

#define IDMAP_START		0xa0000

#define PGDIR_ADDR		0xfffff000
#define PGTBL_BASE		(0xffffffff - 4096 * 1024 + 1)
#define PGTBL(x)		((uint32_t*)(PGTBL_BASE + PGSIZE * (x)))

#define ATTR_PGDIR_MASK	0x3f
#define ATTR_PGTBL_MASK	0x1ff

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
static void pgfault(int inum);
static int copy_on_write(struct vm_page *page);
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


void init_vm(void)
{
	uint32_t idmap_end;
	int i, kmem_start_pg, pgtbl_base_pg;

	/* setup the page tables */
	pgdir = (uint32_t*)alloc_phys_page();
	memset(pgdir, 0, PGSIZE);
	set_pgdir_addr((uint32_t)pgdir);

	/* map the video memory and kernel code 1-1 */
	get_kernel_mem_range(0, &idmap_end);
	map_mem_range(IDMAP_START, idmap_end - IDMAP_START, IDMAP_START, 0);

	/* make the last page directory entry point to the page directory */
	pgdir[1023] = ((uint32_t)pgdir & PGENT_ADDR_MASK) | PG_PRESENT;
	pgdir = (uint32_t*)PGDIR_ADDR;

	/* set the page fault handler */
	interrupt(PAGEFAULT, pgfault);

	/* we can enable paging now */
	enable_paging();

	/* initialize the virtual page allocator */
	node_pool = 0;

	kmem_start_pg = ADDR_TO_PAGE(KMEM_START);
	pgtbl_base_pg = ADDR_TO_PAGE(PGTBL_BASE);

	first_node.start = kmem_start_pg;
	first_node.end = pgtbl_base_pg;
	first_node.next = 0;
	pglist[MEM_KERNEL] = &first_node;

	pglist[MEM_USER] = alloc_node();
	pglist[MEM_USER]->start = ADDR_TO_PAGE(idmap_end);
	pglist[MEM_USER]->end = kmem_start_pg;
	pglist[MEM_USER]->next = 0;

	/* temporarily map something into every 1024th page of the kernel address
	 * space to force pre-allocation of all the kernel page-tables
	 */
	for(i=kmem_start_pg; i<pgtbl_base_pg; i+=1024) {
		/* if there's already something mapped here, leave it alone */
		if(virt_to_phys_page(i) == -1) {
			map_page(i, 0, 0);
			unmap_page(i);
		}
	}
}

/* if ppage == -1 we allocate a physical page by calling alloc_phys_page */
int map_page(int vpage, int ppage, unsigned int attr)
{
	uint32_t *pgtbl;
	int diridx, pgidx, pgon, intr_state;
	struct process *p;

	intr_state = get_intr_state();
	disable_intr();

	pgon = get_paging_status();

	if(ppage < 0) {
		uint32_t addr = alloc_phys_page();
		if(!addr) {
			set_intr_state(intr_state);
			return -1;
		}
		ppage = ADDR_TO_PAGE(addr);
	}

	diridx = PAGE_TO_PGTBL(vpage);
	pgidx = PAGE_TO_PGTBL_PG(vpage);

	if(!(pgdir[diridx] & PG_PRESENT)) {
		/* no page table present, we must allocate one */
		uint32_t addr = alloc_phys_page();

		/* make sure all page directory entries in the below the kernel vm
		 * split have the user and writable bits set, otherwise further user
		 * mappings on the same 4mb block will be unusable in user space.
		 */
		unsigned int pgdir_attr = attr;
		if(vpage < ADDR_TO_PAGE(KMEM_START)) {
			pgdir_attr |= PG_USER | PG_WRITABLE;
		}

		pgdir[diridx] = addr | (pgdir_attr & ATTR_PGDIR_MASK) | PG_PRESENT;

		pgtbl = pgon ? PGTBL(diridx) : (uint32_t*)addr;
		memset(pgtbl, 0, PGSIZE);
	} else {
		if(pgon) {
			pgtbl = PGTBL(diridx);
		} else {
			pgtbl = (uint32_t*)(pgdir[diridx] & PGENT_ADDR_MASK);
		}
	}

	pgtbl[pgidx] = PAGE_TO_ADDR(ppage) | (attr & ATTR_PGTBL_MASK) | PG_PRESENT;
	flush_tlb_page(vpage);

	/* if it's a new *user* mapping, and there is a current process, update the vmmap */
	if((attr & PG_USER) && (p = get_current_proc())) {
		struct vm_page *page;

		if(!(page = get_vm_page_proc(p, vpage))) {
			if(!(page = malloc(sizeof *page))) {
				panic("map_page: failed to allocate new vm_page structure");
			}
			page->vpage = vpage;
			page->ppage = ppage;
			page->flags = (attr & ATTR_PGTBL_MASK) | PG_PRESENT;
			page->nref = 1;

			rb_inserti(&p->vmmap, vpage, page);
		} else {
			/* otherwise just update the mapping */
			page->ppage = ppage;

			/* XXX don't touch the flags, as that's how we implement CoW
			 * by changing the mapping without affecting the vm_page
			 */
		}
	}

	set_intr_state(intr_state);
	return 0;
}

int unmap_page(int vpage)
{
	uint32_t *pgtbl;
	int res = 0;
	int diridx = PAGE_TO_PGTBL(vpage);
	int pgidx = PAGE_TO_PGTBL_PG(vpage);

	int intr_state = get_intr_state();
	disable_intr();

	if(!(pgdir[diridx] & PG_PRESENT)) {
		goto err;
	}
	pgtbl = PGTBL(diridx);

	if(!(pgtbl[pgidx] & PG_PRESENT)) {
		goto err;
	}
	pgtbl[pgidx] = 0;
	flush_tlb_page(vpage);

	if(0) {
err:
		printf("unmap_page(%d): page already not mapped\n", vpage);
		res = -1;
	}
	set_intr_state(intr_state);
	return res;
}

/* if ppg_start is -1, we allocate physical pages to map with alloc_phys_page() */
int map_page_range(int vpg_start, int pgcount, int ppg_start, unsigned int attr)
{
	int i, phys_pg;

	for(i=0; i<pgcount; i++) {
		phys_pg = ppg_start < 0 ? -1 : ppg_start + i;
		map_page(vpg_start + i, phys_pg, attr);
	}
	return 0;
}

int unmap_page_range(int vpg_start, int pgcount)
{
	int i, res = 0;

	for(i=0; i<pgcount; i++) {
		if(unmap_page(vpg_start + i) == -1) {
			res = -1;
		}
	}
	return res;
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

/* translate a virtual address to a physical address using the current page table */
uint32_t virt_to_phys(uint32_t vaddr)
{
	int pg;
	uint32_t pgaddr;

	if((pg = virt_to_phys_page(ADDR_TO_PAGE(vaddr))) == -1) {
		return 0;
	}
	pgaddr = PAGE_TO_ADDR(pg);

	return pgaddr | ADDR_TO_PGOFFS(vaddr);
}

/* translate a virtual page number to a physical page number using the current page table */
int virt_to_phys_page(int vpg)
{
	uint32_t pgaddr, *pgtbl;
	int diridx, pgidx;

	if(vpg < 0 || vpg >= PAGE_COUNT) {
		return -1;
	}

	diridx = PAGE_TO_PGTBL(vpg);
	pgidx = PAGE_TO_PGTBL_PG(vpg);

	if(!(pgdir[diridx] & PG_PRESENT)) {
		return -1;
	}
	pgtbl = PGTBL(diridx);

	if(!(pgtbl[pgidx] & PG_PRESENT)) {
		return -1;
	}
	pgaddr = pgtbl[pgidx] & PGENT_ADDR_MASK;
	return ADDR_TO_PAGE(pgaddr);
}

/* same as virt_to_phys, but uses the vm_page tree instead of the actual page table */
uint32_t virt_to_phys_proc(struct process *p, uint32_t vaddr)
{
	int pg;
	uint32_t pgaddr;

	if((pg = virt_to_phys_page_proc(p, ADDR_TO_PAGE(vaddr))) == -1) {
		return 0;
	}
	pgaddr = PAGE_TO_ADDR(pg);

	return pgaddr | ADDR_TO_PGOFFS(vaddr);
}

/* same virt_to_phys_page, but uses the vm_page tree instead of the actual page table */
int virt_to_phys_page_proc(struct process *p, int vpg)
{
	struct rbnode *node;
	assert(p);

	if(!(node = rb_findi(&p->vmmap, vpg))) {
		return -1;
	}
	return ((struct vm_page*)node->data)->ppage;
}

/* allocate a contiguous block of virtual memory pages along with
 * backing physical memory for them, and update the page table.
 */
int pgalloc(int num, int area)
{
	int intr_state, ret = -1;
	struct page_range *node, *prev, dummy;

	intr_state = get_intr_state();
	disable_intr();

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
		/*unsigned int attr = (area == MEM_USER) ? (PG_USER | PG_WRITABLE) : PG_GLOBAL;*/
		unsigned int attr = (area == MEM_USER) ? (PG_USER | PG_WRITABLE) : 0;

		/* allocate physical storage and map */
		if(map_page_range(ret, num, -1, attr) == -1) {
			ret = -1;
		}
	}

	set_intr_state(intr_state);
	return ret;
}

int pgalloc_vrange(int start, int num)
{
	struct page_range *node, *prev, dummy;
	int area, intr_state, ret = -1;

	area = (start >= ADDR_TO_PAGE(KMEM_START)) ? MEM_KERNEL : MEM_USER;
	if(area == MEM_USER && start + num > ADDR_TO_PAGE(KMEM_START)) {
		printf("pgalloc_vrange: invalid range request crossing user/kernel split\n");
		return -1;
	}

	intr_state = get_intr_state();
	disable_intr();

	dummy.next = pglist[area];
	node = pglist[area];
	prev = &dummy;

	/* check to see if the requested VM range is available */
	node = pglist[area];
	while(node) {
		if(start >= node->start && start + num <= node->end) {
			ret = start;	/* can do .. */

			if(start == node->start) {
				/* adjacent to the start of the range */
				node->start += num;
			} else if(start + num == node->end) {
				/* adjacent to the end of the range */
				node->end = start;
			} else {
				/* somewhere in the middle, which means we need
				 * to allocate a new page_range
				 */
				struct page_range *newnode;

				if(!(newnode = alloc_node())) {
					panic("pgalloc_vrange failed to allocate new page_range while splitting a range in half... bummer\n");
				}
				newnode->start = start + num;
				newnode->end = node->end;
				newnode->next = node->next;

				node->end = start;
				node->next = newnode;
				/* no need to check for null nodes at this point, there's
				 * certainly stuff at the begining and the end, otherwise we
				 * wouldn't be here. so break out of it.
				 */
				break;
			}

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
		/*unsigned int attr = (area == MEM_USER) ? (PG_USER | PG_WRITABLE) : PG_GLOBAL;*/
		unsigned int attr = (area == MEM_USER) ? (PG_USER | PG_WRITABLE) : 0;

		/* allocate physical storage and map */
		if(map_page_range(ret, num, -1, attr) == -1) {
			ret = -1;
		}
	}

	set_intr_state(intr_state);
	return ret;
}

void pgfree(int start, int num)
{
	int i, area, intr_state;
	struct page_range *node, *new, *prev, *next;

	intr_state = get_intr_state();
	disable_intr();

	for(i=0; i<num; i++) {
		int phys_pg = virt_to_phys_page(start + i);
		if(phys_pg != -1) {
			free_phys_page(phys_pg);
		}
	}

	if(!(new = alloc_node())) {
		panic("pgfree: can't allocate new page_range node to add the freed pages\n");
	}
	new->start = start;
	new->end = start + num;

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
	set_intr_state(intr_state);
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

static void pgfault(int inum)
{
	struct intr_frame *frm = get_intr_frame();
	uint32_t fault_addr = get_fault_addr();

	/* the fault occured in user space */
	if(frm->err & PG_USER) {
		int fault_page = ADDR_TO_PAGE(fault_addr);
		struct process *proc = get_current_proc();
		printf("DBG: page fault in user space (pid: %d)\n", proc->id);
		assert(proc);

		if(frm->err & PG_PRESENT) {
			/* it's not due to a missing page fetch the attributes */
			int pgnum = ADDR_TO_PAGE(fault_addr);

			if((frm->err & PG_WRITABLE) && (get_page_bit(pgnum, PG_WRITABLE, 0) == 0)) {
				/* write permission fault might be a CoW fault or just an error
				 * fetch the vm_page permissions to check if this is suppoosed to be
				 * a writable page (which means we should CoW).
				 */
				struct vm_page *page = get_vm_page_proc(proc, pgnum);

				if(page->flags & PG_WRITABLE) {
					/* ok this is a CoW fault */
					if(copy_on_write(page) == -1) {
						panic("copy on write failed!");
					}
					return;	/* done, allow the process to restart the instruction and continue */
				} else {
					/* TODO eventually we'll SIGSEGV the process, for now just panic.
					 */
					goto unhandled;
				}
			}
			goto unhandled;
		}

		/* so it's a missing page... ok */

		/* detect if it's an automatic stack growth deal */
		if(fault_page < proc->user_stack_pg && proc->user_stack_pg - fault_page < USTACK_MAXGROW) {
			int num_pages = proc->user_stack_pg - fault_page;
			printf("growing user (%d) stack by %d pages\n", proc->id, num_pages);

			if(pgalloc_vrange(fault_page, num_pages) != fault_page) {
				printf("failed to allocate VM for stack growth\n");
				/* TODO: in the future we'd SIGSEGV the process here, for now just panic */
				goto unhandled;
			}
			proc->user_stack_pg = fault_page;
			return;
		}

		/* it's not a stack growth fault. since we don't do swapping yet, just
		 * fall to unhandled and panic
		 */
	}

unhandled:
	printf("~~~~ PAGE FAULT ~~~~\n");
	printf("fault address: %x\n", fault_addr);
	printf("error code: %x\n", frm->err);

	if(frm->err & PG_PRESENT) {
		if(frm->err & 8) {
			printf("reserved bit set in some paging structure\n");
		} else {
			printf("%s protection violation ", (frm->err & PG_WRITABLE) ? "WRITE" : "READ");
			printf("in %s mode\n", (frm->err & PG_USER) ? "user" : "kernel");
		}
	} else {
		printf("page not present\n");
	}

	panic("unhandled page fault\n");
}

/* copy-on-write handler, called from pgfault above */
static int copy_on_write(struct vm_page *page)
{
	int tmpvpg;
	struct vm_page *newpage;
	struct rbnode *vmnode;
	struct process *p = get_current_proc();

	assert(page->nref > 0);

	/* first of all check the refcount. If it's 1 then we don't need to copy
	 * anything. This will happen when all forked processes except one have
	 * marked this read-write again after faulting.
	 */
	if(page->nref == 1) {
		set_page_bit(page->vpage, PG_WRITABLE, PAGE_ONLY);
		return 0;
	}

	/* ok let's make a copy and mark it read-write */
	if(!(newpage = malloc(sizeof *newpage))) {
		printf("copy_on_write: failed to allocate new vm_page\n");
		return -1;
	}
	newpage->vpage = page->vpage;
	newpage->flags = page->flags;

	if(!(tmpvpg = pgalloc(1, MEM_KERNEL))) {
		printf("copy_on_write: failed to allocate physical page\n");
		/* XXX proper action: SIGSEGV */
		return -1;
	}
	newpage->ppage = virt_to_phys_page(tmpvpg);
	newpage->nref = 1;

	/* do the copy */
	memcpy((void*)PAGE_TO_ADDR(tmpvpg), (void*)PAGE_TO_ADDR(page->vpage), PGSIZE);
	unmap_page(tmpvpg);
	pgfree(tmpvpg, 1);

	/* set the new vm_page in the process vmmap */
	vmnode = rb_findi(&p->vmmap, newpage->vpage);
	assert(vmnode && vmnode->data == page);	/* shouldn't be able to fail */
	vmnode->data = newpage;

	/* also update tha page table */
	map_page(newpage->vpage, newpage->ppage, newpage->flags);

	/* finally decrease the refcount at the original vm_page struct */
	page->nref--;
	return 0;
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
		/*printf("alloc_node -> %x\n", (unsigned int)node);*/
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
	/*printf("alloc_node -> %x\n", (unsigned int)node);*/
	return node;
}

static void free_node(struct page_range *node)
{
	node->next = node_pool;
	node_pool = node;
	/*printf("free_node\n");*/
}

/* clone_vm makes a copy of the current page tables, thus duplicating the
 * virtual address space.
 *
 * For the kernel part of the address space (last 256 page directory entries)
 * we don't want to diplicate the page tables, just point all page directory
 * entries to the same set of page tables.
 *
 * If "cow" is non-zero it also marks the shared user-space pages as
 * read-only, to implement copy-on-write.
 */
void clone_vm(struct process *pdest, struct process *psrc, int cow)
{
	int i, j, dirpg, tblpg, kstart_dirent;
	uint32_t paddr;
	uint32_t *ndir, *ntbl;
	struct rbnode *vmnode;

	/* allocate the new page directory */
	if((dirpg = pgalloc(1, MEM_KERNEL)) == -1) {
		panic("clone_vmem: failed to allocate page directory page\n");
	}
	ndir = (uint32_t*)PAGE_TO_ADDR(dirpg);

	/* allocate a virtual page for temporarily mapping all new
	 * page tables while we populate them.
	 */
	if((tblpg = pgalloc(1, MEM_KERNEL)) == -1) {
		panic("clone_vmem: failed to allocate page table page\n");
	}
	ntbl = (uint32_t*)PAGE_TO_ADDR(tblpg);

	/* we will allocate physical pages and map them to this virtual page
	 * as needed in the loop below. we don't need the physical page allocated
	 * by pgalloc.
	 */
	free_phys_page(virt_to_phys((uint32_t)ntbl));

	kstart_dirent = ADDR_TO_PAGE(KMEM_START) / 1024;

	/* user space */
	for(i=0; i<kstart_dirent; i++) {
		if(pgdir[i] & PG_PRESENT) {
			if(cow) {
				/* first go through all the entries of the existing
				 * page table and unset the writable bits.
				 */
				for(j=0; j<1024; j++) {
					if(PGTBL(i)[j] & PG_PRESENT) {
						clear_page_bit(i * 1024 + j, PG_WRITABLE, PAGE_ONLY);
						/*PGTBL(i)[j] &= ~(uint32_t)PG_WRITABLE;*/
					}
				}
			}

			/* allocate a page table for the clone */
			paddr = alloc_phys_page();

			/* copy the page table */
			map_page(tblpg, ADDR_TO_PAGE(paddr), 0);
			memcpy(ntbl, PGTBL(i), PGSIZE);

			/* set the new page directory entry */
			ndir[i] = paddr | (pgdir[i] & PGOFFS_MASK);
		} else {
			ndir[i] = 0;
		}
	}

	/* make a copy of the parent's vmmap tree pointing to the same vm_pages
	 * and increase the reference counters for all vm_pages.
	 */
	rb_init(&pdest->vmmap, RB_KEY_INT);
	rb_begin(&psrc->vmmap);
	while((vmnode = rb_next(&psrc->vmmap))) {
		struct vm_page *pg = vmnode->data;
		pg->nref++;

		/* insert the same vm_page to the new tree */
		rb_inserti(&pdest->vmmap, pg->vpage, pg);
	}

	/* for the kernel space we'll just use the same page tables */
	for(i=kstart_dirent; i<1023; i++) {
		ndir[i] = pgdir[i];
	}

	/* also point the last page directory entry to the page directory address
	 * since we're relying on recursive page tables
	 */
	paddr = virt_to_phys((uint32_t)ndir);
	ndir[1023] = paddr | PG_PRESENT;

	if(cow) {
		/* we just changed all the page protection bits, so we need to flush the TLB */
		flush_tlb();
	}

	/* unmap before freeing the virtual pages, to avoid deallocating the physical pages */
	unmap_page(dirpg);
	unmap_page(tblpg);

	pgfree(dirpg, 1);
	pgfree(tblpg, 1);

	/* set the new page directory pointer */
	pdest->ctx.pgtbl_paddr = paddr;
}

/* cleanup_vm called by exit to clean up any memory used by the process */
void cleanup_vm(struct process *p)
{
	struct rbnode *vmnode;

	/* go through the vm map and reduce refcounts all around
	 * when a ref goes to 0, free the physical page
	 */
	rb_begin(&p->vmmap);
	while((vmnode = rb_next(&p->vmmap))) {
		struct vm_page *page = vmnode->data;

		/* skip kernel pages obviously */
		if(!(page->flags & PG_USER)) {
			continue;
		}

		if(--page->nref <= 0) {
			/* free the physical page if nref goes to 0 */
			free_phys_page(PAGE_TO_ADDR(page->ppage));
		}
	}

	/* destroying the tree will free the nodes */
	rb_destroy(&p->vmmap);
}


int get_page_bit(int pgnum, uint32_t bit, int wholepath)
{
	int tidx = PAGE_TO_PGTBL(pgnum);
	int tent = PAGE_TO_PGTBL_PG(pgnum);
	uint32_t *pgtbl = PGTBL(tidx);

	if(wholepath) {
		if((pgdir[tidx] & bit) == 0) {
			return 0;
		}
	}

	return pgtbl[tent] & bit;
}

void set_page_bit(int pgnum, uint32_t bit, int wholepath)
{
	int tidx = PAGE_TO_PGTBL(pgnum);
	int tent = PAGE_TO_PGTBL_PG(pgnum);
	uint32_t *pgtbl = PGTBL(tidx);

	if(wholepath) {
		pgdir[tidx] |= bit;
	}
	pgtbl[tent] |= bit;

	flush_tlb_page(pgnum);
}

void clear_page_bit(int pgnum, uint32_t bit, int wholepath)
{
	int tidx = PAGE_TO_PGTBL(pgnum);
	int tent = PAGE_TO_PGTBL_PG(pgnum);
	uint32_t *pgtbl = PGTBL(tidx);

	if(wholepath) {
		pgdir[tidx] &= ~bit;
	}

	pgtbl[tent] &= ~bit;

	flush_tlb_page(pgnum);
}


#define USER_PGDIR_ENTRIES	PAGE_TO_PGTBL(KMEM_START_PAGE)
int cons_vmmap(struct rbtree *vmmap)
{
	int i, j;

	rb_init(vmmap, RB_KEY_INT);

	for(i=0; i<USER_PGDIR_ENTRIES; i++) {
		if(pgdir[i] & PG_PRESENT) {
			/* page table is present, iterate through its 1024 pages */
			uint32_t *pgtbl = PGTBL(i);

			for(j=0; j<1024; j++) {
				if(pgtbl[j] & PG_PRESENT) {
					struct vm_page *vmp;

					if(!(vmp = malloc(sizeof *vmp))) {
						panic("cons_vmap failed to allocate memory");
					}
					vmp->vpage = i * 1024 + j;
					vmp->ppage = ADDR_TO_PAGE(pgtbl[j] & PGENT_ADDR_MASK);
					vmp->flags = pgtbl[j] & ATTR_PGTBL_MASK;
					vmp->nref = 1;	/* when first created assume no sharing */

					rb_inserti(vmmap, vmp->vpage, vmp);
				}
			}
		}
	}

	return 0;
}

struct vm_page *get_vm_page(int vpg)
{
	return get_vm_page_proc(get_current_proc(), vpg);
}

struct vm_page *get_vm_page_proc(struct process *p, int vpg)
{
	struct rbnode *node;

	if(!p || !(node = rb_findi(&p->vmmap, vpg))) {
		return 0;
	}
	return node->data;
}


void dbg_print_vm(int area)
{
	struct page_range *node;
	int last, intr_state;

	intr_state = get_intr_state();
	disable_intr();

	node = pglist[area];
	last = area == MEM_USER ? 0 : ADDR_TO_PAGE(KMEM_START);

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

	set_intr_state(intr_state);
}
