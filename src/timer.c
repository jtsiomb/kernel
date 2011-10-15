#include <stdio.h>
#include <time.h>
#include "intr.h"
#include "asmops.h"
#include "timer.h"
#include "proc.h"
#include "sched.h"
#include "config.h"

/* frequency of the oscillator driving the 8254 timer */
#define OSC_FREQ_HZ		1193182

/* macro to divide and round to the nearest integer */
#define DIV_ROUND(a, b) ((a) / (b) + ((a) % (b)) / ((b) / 2))

/* I/O ports connected to the 8254 */
#define PORT_DATA0	0x40
#define PORT_DATA1	0x41
#define PORT_DATA2	0x42
#define PORT_CMD	0x43

/* command bits */
#define CMD_CHAN0			0
#define CMD_CHAN1			(1 << 6)
#define CMD_CHAN2			(2 << 6)
#define CMD_RDBACK			(3 << 6)

#define CMD_LATCH			0
#define CMD_ACCESS_LOW		(1 << 4)
#define CMD_ACCESS_HIGH		(2 << 4)
#define CMD_ACCESS_BOTH		(3 << 4)

#define CMD_OP_INT_TERM		0
#define CMD_OP_ONESHOT		(1 << 1)
#define CMD_OP_RATE			(2 << 1)
#define CMD_OP_SQWAVE		(3 << 1)
#define CMD_OP_SOFT_STROBE	(4 << 1)
#define CMD_OP_HW_STROBE	(5 << 1)

#define CMD_MODE_BIN		0
#define CMD_MODE_BCD		1


#define MSEC_TO_TICKS(ms)	((ms) * TICK_FREQ_HZ / 1000)

struct timer_event {
	int dt;	/* remaining ticks delta from the previous event */
	struct timer_event *next;
};


static void timer_handler();


static struct timer_event *evlist;


void init_timer(void)
{
	/* calculate the reload count: round(osc / freq) */
	int reload_count = DIV_ROUND(OSC_FREQ_HZ, TICK_FREQ_HZ);

	/* set the mode to square wave for channel 0, both low
	 * and high reload count bytes will follow...
	 */
	outb(CMD_CHAN0 | CMD_ACCESS_BOTH | CMD_OP_SQWAVE, PORT_CMD);

	/* write the low and high bytes of the reload count to the
	 * port for channel 0
	 */
	outb(reload_count & 0xff, PORT_DATA0);
	outb((reload_count >> 8) & 0xff, PORT_DATA0);

	/* set the timer interrupt handler */
	interrupt(IRQ_TO_INTR(0), timer_handler);
}

int sys_sleep(int sec)
{
	printf("process %d will sleep for %d seconds\n", get_current_pid(), sec);
	sleep(sec * 1000); /* timer.c */

	/* TODO if interrupted, return the remaining seconds */
	return 0;
}

void sleep(unsigned long msec)
{
	int ticks, tsum, istate;
	struct timer_event *ev, *node;

	if((ticks = MSEC_TO_TICKS(msec)) <= 0) {
		return;
	}

	if(!(ev = malloc(sizeof *ev))) {
		printf("sleep: failed to allocate timer_event structure\n");
		return;
	}

	istate = get_intr_state();
	disable_intr();

	/* insert at the beginning */
	if(!evlist || ticks <= evlist->dt) {
		ev->next = evlist;
		evlist = ev;

		ev->dt = ticks;
		if(ev->next) {
			ev->next->dt -= ticks;
		}
	} else {

		tsum = evlist->dt;
		node = evlist;

		while(node->next && ticks > tsum + node->next->dt) {
			tsum += node->next->dt;
			node = node->next;
		}

		ev->next = node->next;
		node->next = ev;

		/* fix the relative times */
		ev->dt = ticks - tsum;
		if(ev->next) {
			ev->next->dt -= ev->dt;
		}
	}

	set_intr_state(istate);

	/* wait on the address of this timer event */
	wait(ev);
}

/* This will be called by the interrupt dispatcher approximately
 * every 1/250th of a second, so it must be extremely fast.
 * For now, just increasing a tick counter will suffice.
 */
static void timer_handler(int inum)
{
	int istate;
	struct process *p;

	nticks++;

	/*printf("TICKS: %d\n", nticks);*/

	istate = get_intr_state();
	disable_intr();

	/* find out if there are any timers that have to go off */
	if(evlist) {
		evlist->dt--;

		while(evlist && evlist->dt <= 0) {
			struct timer_event *ev = evlist;
			evlist = evlist->next;

			printf("timer going off!!!\n");
			/* wake up all processes waiting on this address */
			wakeup(ev);
			free(ev);
		}
	}

	/* decrement the process' ticks_left and call the scheduler to decide if
	 * it's time to switch processes
	 */
	if((p = get_current_proc())) {
		p->ticks_left--;
	}
	schedule();

	set_intr_state(istate);
}
