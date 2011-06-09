#include <string.h>
#include "vid.h"
#include "intr.h"
#include "asmops.h"

/* height of our virtual console text buffer */
#define VIRT_HEIGHT	200

/* CRTC ports */
#define CRTC_ADDR	0x3d4
#define CRTC_DATA	0x3d5

/* CRTC registers */
#define CRTC_START_HIGH		0xc
#define CRTC_START_LOW		0xd
#define CRTC_CURSOR_HIGH	0xe
#define CRTC_CURSOR_LOW		0xf

/* construct a character with its attributes */
#define VMEM_CHAR(c, fg, bg) \
	((uint16_t)(c) | (((uint16_t)(fg) & 0xf) << 8) | (((uint16_t)(bg) & 0xf) << 12))

static void set_start_line(int line);

static uint16_t *vmem = (uint16_t*)0xb8000;
static int start_line;


void clear_scr(void)
{
	int istate = get_intr_state();
	disable_intr();

	memset16(vmem, VMEM_CHAR(' ', LTGRAY, BLACK), WIDTH * HEIGHT);
	start_line = 0;
	set_start_line(0);
	set_cursor(0, 0);

	set_intr_state(istate);
}

void set_char(char c, int x, int y, int fg, int bg)
{
	vmem[(y + start_line) * WIDTH + x] = VMEM_CHAR(c, fg, bg);
}

void set_cursor(int x, int y)
{
	int loc;
	int istate = get_intr_state();
	disable_intr();

	if(x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
		loc = 0xffff;
	} else {
		loc = (y + start_line) * WIDTH + x;
	}

	outb(CRTC_CURSOR_LOW, CRTC_ADDR);
	outb(loc, CRTC_DATA);
	outb(CRTC_CURSOR_HIGH, CRTC_ADDR);
	outb(loc >> 8, CRTC_DATA);

	set_intr_state(istate);
}

void scroll_scr(void)
{
	int new_line, istate = get_intr_state();
	disable_intr();

	if(++start_line > VIRT_HEIGHT - HEIGHT) {
		/* The bottom of the visible range reached the end of our text buffer.
		 * Copy the rest of the lines to the top and reset start_line.
		 */
		memcpy(vmem, vmem + start_line * WIDTH, (HEIGHT - 1) * WIDTH * 2);
		start_line = 0;
	}

	/* clear the next line that will be revealed by scrolling */
	new_line = start_line + HEIGHT - 1;
	memset16(vmem + new_line * WIDTH, VMEM_CHAR(' ', LTGRAY, BLACK), WIDTH);
	set_start_line(start_line);

	set_intr_state(istate);
}

static void set_start_line(int line)
{
	int start_addr = line * WIDTH;

	outb(CRTC_START_LOW, CRTC_ADDR);
	outb(start_addr & 0xff, CRTC_DATA);
	outb(CRTC_START_HIGH, CRTC_ADDR);
	outb((start_addr >> 8) & 0xff, CRTC_DATA);
}
