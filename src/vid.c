#include <string.h>
#include "vid.h"

#define WIDTH	80
#define HEIGHT	25
static uint16_t *vmem = (uint16_t*)0xb8000;

void clear_scr(void)
{
	memset(vmem, 0, WIDTH * HEIGHT * 2);
}

void set_cursor(int x, int y)
{
	if(x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
		/* disable cursor */
		return;
	}
	/* set cursor position */
}

void put_char(char c, int x, int y, int fg, int bg)
{
	uint16_t attr = (fg & 0xf) | ((bg & 7) << 4);
	vmem[y * 80 + x] = (uint16_t)c | (attr << 8);
}
