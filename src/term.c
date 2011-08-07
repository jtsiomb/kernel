#include <ctype.h>
#include "term.h"
#include "vid.h"
#include "intr.h"

static int bg, fg = LTGRAY;
static int cursor_x, cursor_y;

/* sets the active text color and returns previous value */
int set_text_color(int c)
{
	int prev = fg;

	if(c >= 0 && c < 16) {
		fg = c;
	}
	return prev;
}

/* sets the active background color and returns the current value */
int set_back_color(int c)
{
	int prev = bg;

	if(c >= 0 && c < 16) {
		bg = c;
	}
	return prev;
}

/* output a single character, handles formatting, cursor advancement
 * and scrolling the screen when we reach the bottom.
 */
int putchar(int c)
{
	int istate = get_intr_state();
	disable_intr();

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
		if(isprint(c)) {
			set_char(c, cursor_x, cursor_y, fg, bg);
			if(++cursor_x >= WIDTH) {
				cursor_x = 0;
				cursor_y++;
			}
		}
	}

	if(cursor_y >= HEIGHT) {
		scroll_scr();
		cursor_y--;
	}

	set_cursor(cursor_x, cursor_y);

	set_intr_state(istate);
	return c;
}
