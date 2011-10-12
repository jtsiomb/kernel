#ifndef VM_H_
#define VM_H_

#include <stdlib.h>
#include "mboot.h"
#include "rbtree.h"

#define KMEM_START		0xc0000000
#define KMEM_START_PAGE	ADDR_TO_PAGE(KMEM_START)

/* page mapping flags */
#define PG_PRESENT			(1 << 0)
#define PG_WRITABLE			(1 << 1)
#define PG_USER				(1 << 2)
#define PG_WRITE_THROUGH	(1 << 3)
#define PG_NOCACHE			(1 << 4)
#define PG_ACCESSED			(1 << 5)
#define PG_DIRTY			(1 << 6)
#define PG_TYPE				(1 << 7)
/* PG_GLOBAL mappings won't flush from TLB */
#define PG_GLOBAL			(1 << 8)


#define PGSIZE					4096
#define PAGE_COUNT				(1024 * 1024)

#define PGOFFS_MASK				0xfff
#define PGNUM_MASK				0xfffff000
#define PGENT_ADDR_MASK			PGNUM_MASK

#define ADDR_TO_PAGE(x)			((uint32_t)(x) >> 12)
#define PAGE_TO_ADDR(x)			((uint32_t)(x) << 12)

#define ADDR_TO_PGTBL(x)		((uint32_t)(x) >> 22)
#define ADDR_TO_PGTBL_PG(x)		(((uint32_t)(x) >> 12) & 0x3ff)
#define ADDR_TO_PGOFFS(x)		((uint32_t)(x) & PGOFFS_MASK)

#define PAGE_TO_PGTBL(x)		((uint32_t)(x) >> 10)
#define PAGE_TO_PGTBL_PG(x)		((uint32_t)(x) & 0x3ff)

/* argument to clone_vm */
#define CLONE_SHARED	0
#define CLONE_COW		1

/* last argument to *_page_bit */
#define PAGE_ONLY		0
#define WHOLE_PATH		1

struct vm_page {
	int vpage, ppage;
	unsigned int flags;

	int nref;
};

struct process;

void init_vm(void);

int map_page(int vpage, int ppage, unsigned int attr);
int unmap_page(int vpage);
int map_page_range(int vpg_start, int pgcount, int ppg_start, unsigned int attr);
int unmap_page_range(int vpg_start, int pgcount);
int map_mem_range(uint32_t vaddr, size_t sz, uint32_t paddr, unsigned int attr);

uint32_t virt_to_phys(uint32_t vaddr);
int virt_to_phys_page(int vpg);

uint32_t virt_to_phys_proc(struct process *p, uint32_t vaddr);
int virt_to_phys_page_proc(struct process *p, int vpg);

enum {
	MEM_KERNEL,
	MEM_USER
};

int pgalloc(int num, int area);
int pgalloc_vrange(int start, int num);
void pgfree(int start, int num);

void clone_vm(struct process *pdest, struct process *psrc, int cow);

int get_page_bit(int pgnum, uint32_t bit, int wholepath);
void set_page_bit(int pgnum, uint32_t bit, int wholepath);
void clear_page_bit(int pgnum, uint32_t bit, int wholepath);

/* construct the vm map for the current user mappings */
int cons_vmmap(struct rbtree *vmmap);

struct vm_page *get_vm_page(int vpg);
struct vm_page *get_vm_page_proc(struct process *p, int vpg);

void dbg_print_vm(int area);

/* defined in vm-asm.S */
void set_pgdir_addr(uint32_t addr);
uint32_t get_pgdir_addr(void);

#endif	/* VM_H_ */
