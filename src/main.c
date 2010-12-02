#include <stdio.h>
#include "vid.h"
#include "term.h"

void kmain(void)
{
	clear_scr(0);
	set_text_color(LTRED);
	puts("hello world!");
}
