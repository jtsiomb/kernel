#include "term.h"
#include "vid.h"

static int bg, fg = 15;
static int cursor_x, cursor_y;

void set_text_color(int c)
{
	if(c >= 0 && c < 16) {
		fg = c;
	}
}

void set_back_color(int c)
{
	if(c >= 0 && c < 16) {
		bg = c;
	}
}

int putchar(int c)
{
	switch(c) {
	case '\n':
		cursor_y++;

	case '\r':
		cursor_x = 0;
		break;

	case '\b':
		cursor_x--;
		set_char(' ', cursor_x, cursor_y, fg, bg);
		break;

	case '\t':
		cursor_x = ((cursor_x >> 3) + 1) << 3;
		break;

	default:
		set_char(c, cursor_x, cursor_y, fg, bg);
		if(++cursor_x >= WIDTH) {
			cursor_x = 0;
			cursor_y++;
		}
	}

	if(cursor_y >= HEIGHT) {
		scroll_scr();
		cursor_y--;
	}
	return c;
}
