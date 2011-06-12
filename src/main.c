#include <stdio.h>
#include "mboot.h"
#include "vid.h"
#include "term.h"
#include "asmops.h"
#include "segm.h"
#include "intr.h"
#include "rtc.h"
#include "timer.h"
#include "mem.h"
#include "vm.h"

/* special keys */
enum {
	LALT, RALT,
	LCTRL, RCTRL,
	LSHIFT, RSHIFT,
	F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
	CAPSLK, NUMLK, SCRLK, SYSRQ,
	ESC		= 27,
	INSERT, DEL, HOME, END, PGUP, PGDN, LEFT, RIGHT, UP, DOWN,
	NUM_DOT, NUM_ENTER, NUM_PLUS, NUM_MINUS, NUM_MUL, NUM_DIV,
	NUM_0, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
	BACKSP	= 127
};

/* table with rough translations from set 1 scancodes to ASCII-ish */
static int keycodes[] = {
	0, ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',		/* 0 - e */
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',			/* f - 1c */
	LCTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',				/* 1d - 29 */
	LSHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', RSHIFT,			/* 2a - 36 */
	NUM_MUL, LALT, ' ', CAPSLK, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,			/* 37 - 44 */
	NUMLK, SCRLK, NUM_7, NUM_8, NUM_9, NUM_MINUS, NUM_4, NUM_5, NUM_6, NUM_PLUS,	/* 45 - 4e */
	NUM_1, NUM_2, NUM_3, NUM_0, NUM_DOT, SYSRQ, 0, 0, F11, F12,						/* 4d - 58 */
	0, 0, 0, 0, 0, 0, 0,															/* 59 - 5f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,									/* 60 - 6f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0									/* 70 - 7f */
};

void kmain(struct mboot_info *mbinf)
{
	clear_scr();

	/* pointless verbal diarrhea */
	if(mbinf->flags & MB_LDRNAME) {
		printf("loaded by: %s\n", mbinf->boot_loader_name);
	}
	if(mbinf->flags & MB_CMDLINE) {
		printf("kernel command line: %s\n", mbinf->cmdline);
	}

	puts("kernel starting up");

	init_segm();
	init_intr();

	/* initialize the physical memory manager */
	init_mem(mbinf);
	/* initialize paging and the virtual memory manager */
	init_vm();

	/* initialize the timer and RTC */
	init_timer();
	init_rtc();

	/* initialization complete, enable interrupts */
	enable_intr();

	for(;;) {
		char c, keypress;
		do {
			inb(keypress, 0x64);
		} while(!(keypress & 1));
		inb(c, 0x60);
		if(!(c & 0x80)) {
			putchar(keycodes[(int)c]);
		}
	}
}
