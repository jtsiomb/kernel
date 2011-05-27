#include <stdlib.h>
#include "intr.h"
#include "vm.h"
#include "panic.h"

#define MAGIC	0xbaadbeef

struct mem_range {
	uint32_t start;
	size_t size;
	struct mem_range *next;
};

struct alloc_desc {
	size_t size;
	uint32_t magic;
};

static void add_range(struct mem_range *rng);
static void coalesce(struct mem_range *low, struct mem_range *mid, struct mem_range *high);
static struct mem_range *alloc_node(void);
static void free_node(struct mem_range *node);

struct mem_range *free_list;
struct mem_range *node_pool;


void *malloc(size_t sz)
{
	void *res = 0;
	struct mem_range *node, *prev, dummy;
	int intr_state;
	struct alloc_desc *desc;
	size_t alloc_size = sz + sizeof *desc;

	if(!sz) {
		return 0;
	}

	/* entering the critical section, do not disturb */
	intr_state = get_intr_state();
	disable_intr();

find_range:
	prev = &dummy;
	dummy.next = node = free_list;
	while(node) {
		/* find a node in the free_list with enough space ... */
		if(node->size >= alloc_size) {
			/* insert the allocation descriptor at the beginning */
			desc = (void*)node->start;
			desc->size = alloc_size;
			desc->magic = MAGIC;
			res = desc + 1; /* that's what we'll return to the user */

			/* modify the node to reflect the new range after we
			 * grabbed a part at the beginning...
			 */
			node->size -= alloc_size;
			node->start += alloc_size;

			/* if the range represented by this node now has zero size,
			 * remove and free the node (it goes into the node_pool)
			 */
			if(!node->size) {
				prev->next = node->next;
				if(free_list == node) {
					free_list = node->next;
				}
				free_node(node);
			}
			break;
		}
		prev = node;
		node = node->next;
	}

	/* we didn't find a range big enough in the free_list. In that case
	 * we need to allocate some pages, add them to the free_list and try
	 * again.
	 */
	if(!res) {
		struct mem_range *range;
		int pg, pgcount = (PGSIZE - 1 + alloc_size) / PGSIZE;

		if((pg = pgalloc(pgcount, MEM_KERNEL)) == -1) {
			set_intr_state(intr_state);
			return 0;
		}

		range = alloc_node();
		range->start = PAGE_TO_ADDR(pg);
		range->size = pg * PGSIZE;
		add_range(range);
		goto find_range;
	}

	set_intr_state(intr_state);
	return res;
}

void free(void *ptr)
{
	int intr_state;
	struct alloc_desc *desc;
	struct mem_range *rng;

	if(!ptr) return;

	intr_state = get_intr_state();
	disable_intr();

	desc = (struct alloc_desc*)ptr - 1;
	if(desc->magic != MAGIC) {
		panic("free(%x) magic missmatch, invalid address.\n", (unsigned int)ptr);
	}

	rng = alloc_node();
	rng->start = (uint32_t)desc;
	rng->size = desc->size;
	add_range(rng);

	set_intr_state(intr_state);
}

static void add_range(struct mem_range *rng)
{
	struct mem_range *node, *prev = 0;

	if(!free_list || free_list->start > rng->start) {
		rng->next = free_list;
		free_list = rng;

	} else {
		node = free_list;

		while(node) {
			if(!node->next || node->next->start > rng->start) {
				rng->next = node->next;
				node->next = rng;
				prev = node;	/* needed by coalesce after the loop */
				break;
			}

			prev = node;
			node = node->next;
		}
	}

	coalesce(prev, rng, rng->next);
}

static void coalesce(struct mem_range *low, struct mem_range *mid, struct mem_range *high)
{
	if(high) {
		if(mid->start + mid->size >= high->start) {
			mid->size = high->size - mid->start;
			mid->next = high->next;
			free_node(high);
		}
	}

	if(low) {
		if(low->start + low->size >= mid->start) {
			low->size = mid->size - low->start;
			low->next = mid->next;
			free_node(mid);
		}
	}
}

static struct mem_range *alloc_node(void)
{
	struct mem_range *node;

	/* no nodes available for reuse...
	 * grab a page, slice it into nodes, link them up and hang them in the pool
	 */
	if(!node_pool) {
		int i, num_nodes, pg;
		struct mem_range *nodepage;

		pg = pgalloc(1, MEM_KERNEL);
		if(pg == -1) {
			panic("failed to allocate page for the malloc node pool\n");
			return 0;	/* unreachable */
		}

		nodepage = (struct mem_range*)PAGE_TO_ADDR(pg);
		num_nodes = PGSIZE / sizeof *nodepage;

		for(i=1; i<num_nodes; i++) {
			nodepage[i - 1].next = nodepage + i;
		}
		nodepage[i - 1].next = 0;
		node_pool = nodepage;
	}

	node = node_pool;
	node_pool = node->next;
	node->next = 0;
	return node;
}

static void free_node(struct mem_range *node)
{
	node->next = node_pool;
	node_pool = node;
}
