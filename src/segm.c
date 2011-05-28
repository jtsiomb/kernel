#include <string.h>
#include "segm.h"
#include "desc.h"

/* bits for the 3rd 16bt part of the descriptor */
#define BIT_ACCESSED	(1 << 8)
#define BIT_WR			(1 << 9)
#define BIT_RD			(1 << 9)
#define BIT_EXP_DOWN	(1 << 10)
#define BIT_CONFORMING	(1 << 10)
#define BIT_CODE		(1 << 11)
#define BIT_NOSYS		(1 << 12)
#define BIT_PRESENT		(1 << 15)
/* TSS busy bit */
#define BIT_BUSY		(1 << 9)

/* bits for the last 16bit part of the descriptor */
#define BIT_BIG			(1 << 6)
#define BIT_DEFAULT		(1 << 6)
#define BIT_GRAN		(1 << 7)

enum {TYPE_DATA, TYPE_CODE};

#define TSS_TYPE_BITS	0x900
#define TSS_BUSY		BIT_BUSY

static void segm_desc(desc_t *desc, uint32_t base, uint32_t limit, int dpl, int type);
static void task_desc(desc_t *desc, uint32_t base, uint32_t limit, int dpl, unsigned int busy);

/* these functions are implemented in segm-asm.S */
void setup_selectors(uint16_t code, uint16_t data);
void set_gdt(uint32_t addr, uint16_t limit);


/* our global descriptor table */
static desc_t gdt[6];


void init_segm(void)
{
	memset(gdt, 0, sizeof gdt);
	segm_desc(gdt + SEGM_KCODE, 0, 0xffffffff, 0, TYPE_CODE);
	segm_desc(gdt + SEGM_KDATA, 0, 0xffffffff, 0, TYPE_DATA);
	segm_desc(gdt + SEGM_UCODE, 0, 0xffffffff, 3, TYPE_CODE);
	segm_desc(gdt + SEGM_UDATA, 0, 0xffffffff, 3, TYPE_DATA);
	/*task_desc(gdt + SEGM_TASK, 0, 0xffffffff, 3, TSS_BUSY);*/

	set_gdt((uint32_t)gdt, sizeof gdt - 1);

	setup_selectors(selector(SEGM_KCODE, 0), selector(SEGM_KDATA, 0));
}

/* constructs a GDT selector based on index and priviledge level */
uint16_t selector(int idx, int rpl)
{
	return (idx << 3) | (rpl & 3);
}

static void segm_desc(desc_t *desc, uint32_t base, uint32_t limit, int dpl, int type)
{
	desc->d[0] = limit & 0xffff; /* low order 16bits of limit */
	desc->d[1] = base & 0xffff;  /* low order 16bits of base */

	/* third 16bit part contains the last 8 bits of base, the 2 priviledge
	 * level bits starting on bit 13, present flag on bit 15, and type bits
	 * starting from bit 8
	 */
	desc->d[2] = ((base >> 16) & 0xff) | ((dpl & 3) << 13) | BIT_PRESENT |
		BIT_NOSYS | (type == TYPE_DATA ? BIT_WR : (BIT_RD | BIT_CODE));

	/* last 16bit part contains the last nibble of limit, the last byte of
	 * base, and the granularity and deafult/big flags in bits 23 and 22 resp.
	 */
	desc->d[3] = ((limit >> 16) & 0xf) | ((base >> 16) & 0xff00) | BIT_GRAN | BIT_BIG;
}

static void task_desc(desc_t *desc, uint32_t base, uint32_t limit, int dpl, unsigned int busy)
{
	desc->d[0] = limit & 0xffff;
	desc->d[1] = base & 0xffff;

	desc->d[2] = ((base >> 16) & 0xff) | ((dpl & 3) << 13) | BIT_PRESENT |
		TSS_TYPE_BITS | busy;
	desc->d[3] = ((limit >> 16) & 0xf) | ((base >> 16) & 0xff00) | BIT_GRAN;
}
