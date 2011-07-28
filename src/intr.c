#include <stdio.h>
#include "intr.h"
#include "desc.h"
#include "segm.h"
#include "asmops.h"
#include "panic.h"

/* IDT gate descriptor bits */
#define GATE_TASK		(5 << 8)
#define GATE_INTR		(6 << 8)
#define GATE_TRAP		(7 << 8)
#define GATE_DEFAULT	(1 << 11)
#define GATE_PRESENT	(1 << 15)

/* PIC command and data ports */
#define PIC1_CMD	0x20
#define PIC1_DATA	0x21
#define PIC2_CMD	0xa0
#define PIC2_DATA	0xa1

/* PIC initialization command word 1 bits */
#define ICW1_ICW4_NEEDED	(1 << 0)
#define ICW1_SINGLE			(1 << 1)
#define ICW1_INTERVAL4		(1 << 2)
#define ICW1_LEVEL			(1 << 3)
#define ICW1_INIT			(1 << 4)
/* PIC initialization command word 4 bits */
#define ICW4_8086			(1 << 0)
#define ICW4_AUTO_EOI		(1 << 1)
#define ICW4_BUF_SLAVE		(1 << 3) /* 1000 */
#define ICW4_BUF_MASTER		(3 << 2) /* 1100 */
#define ICW4_SPECIAL		(1 << 4)

/* PIC operation command word 2 bits */
#define OCW2_EOI	(1 << 5)


static void init_pic(int offset);
static void gate_desc(desc_t *desc, uint16_t sel, uint32_t addr, int dpl, int type);
static void set_intr_entry(int num, void (*handler)(void));
static void end_of_irq(int irq);

/* defined in intr-asm.S */
void set_idt(uint32_t addr, uint16_t limit);
void intr_entry_default(void);

/* the IDT (interrupt descriptor table) */
static desc_t idt[256];
/* table of handler functions for all interrupts */
static intr_func_t intr_func[256];


void init_intr(void)
{
	int i;

	set_idt((uint32_t)idt, sizeof idt - 1);

	/* initialize all entry points and interrupt handlers */
	for(i=0; i<256; i++) {
		set_intr_entry(i, intr_entry_default);
		interrupt(i, 0);
	}

	/* by including interrupts.h here (without ASM being defined)
	 * the series of INTR_ENTRY_* macros will be expanded to a series
	 * of function prototypes for all interrupt entry points and the
	 * corresponding calls to set_intr_entry to set up the IDT slots
	 */
#include "interrupts.h"

	/* initialize the programmable interrupt controller
	 * setting up the maping of IRQs [0, 15] to interrupts [32, 47]
	 */
	init_pic(IRQ_OFFSET);
}

/* set an interrupt handler function for a particular interrupt */
void interrupt(int intr_num, intr_func_t func)
{
	intr_func[intr_num] = func;
}

/* this function is called from all interrupt entry points
 * it calls the appropriate interrupt handlers if available and handles
 * sending an end-of-interrupt command to the PICs when finished.
 */
void dispatch_intr(struct intr_frame frm)
{
	if(intr_func[frm.inum]) {
		intr_func[frm.inum](frm.inum, frm.err);
	} else {
		if(frm.inum < 32) {
			panic("unhandled exception %d, error code: %d\n", frm.inum, frm.err);
		}
		printf("unhandled interrupt %d\n", frm.inum);
	}

	if(IS_IRQ(frm.inum)) {
		end_of_irq(INTR_TO_IRQ(frm.inum));
	}
}

static void init_pic(int offset)
{
	/* send ICW1 saying we'll follow with ICW4 later on */
	outb(ICW1_INIT | ICW1_ICW4_NEEDED, PIC1_CMD);
	outb(ICW1_INIT | ICW1_ICW4_NEEDED, PIC2_CMD);
	/* send ICW2 with IRQ remapping */
	outb(offset, PIC1_DATA);
	outb(offset + 8, PIC2_DATA);
	/* send ICW3 to setup the master/slave relationship */
	/* ... set bit3 = 3rd interrupt input has a slave */
	outb(4, PIC1_DATA);
	/* ... set slave ID to 2 */
	outb(2, PIC2_DATA);
	/* send ICW4 to set 8086 mode (no calls generated) */
	outb(ICW4_8086, PIC1_DATA);
	outb(ICW4_8086, PIC2_DATA);
	/* done, just reset the data port to 0 */
	outb(0, PIC1_DATA);
	outb(0, PIC2_DATA);
}

static void gate_desc(desc_t *desc, uint16_t sel, uint32_t addr, int dpl, int type)
{
	/* first 16bit part is the low 16bits of the entry address */
	desc->d[0] = addr & 0xffff;
	/* second 16bit part is the segment selector for the entry code */
	desc->d[1] = sel;
	/* third 16bit part has the privilege level, type, and present bit */
	desc->d[2] = ((dpl & 3) << 13) | type | GATE_DEFAULT | GATE_PRESENT;
	/* last 16bit part is the high 16bits of the entry address */
	desc->d[3] = (addr & 0xffff0000) >> 16;
}

#define IS_TRAP(n)	((n) >= 32 && !IS_IRQ(n))
static void set_intr_entry(int num, void (*handler)(void))
{
	int type = IS_TRAP(num) ? GATE_TRAP : GATE_INTR;
	gate_desc(idt + num, selector(SEGM_KCODE, 0), (uint32_t)handler, 0, type);
}

static void end_of_irq(int irq)
{
	if(irq > 7) {
		outb(OCW2_EOI, PIC2_CMD);
	}
	outb(OCW2_EOI, PIC1_CMD);
}
