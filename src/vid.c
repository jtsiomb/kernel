#include <string.h>
#include "vid.h"

static uint16_t *vmem = (uint16_t*)0xb8000;

void clear_scr(int color)
{
	memset(vmem, 0, WIDTH * HEIGHT * 2);
}

void set_char(char c, int x, int y, int fg, int bg)
{
	uint16_t attr = (fg & 0xf) | ((bg & 7) << 4);
	vmem[y * WIDTH + x] = (uint16_t)c | (attr << 8);
}

void scroll_scr(void)
{
	/* copy the second up to last text line, one line back, then
	 * clear the last line.
	 */
	memmove(vmem, vmem + WIDTH, WIDTH * (HEIGHT - 1) * 2);
	memset(vmem + WIDTH * (HEIGHT -1), 0, WIDTH * 2);
}
