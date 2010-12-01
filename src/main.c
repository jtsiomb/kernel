/*#include <stdio.h>*/
#include "vid.h"

void kmain(void)
{
	clear_scr();
	put_char('a', 0, 0, 4, 0);
	put_char('b', 40, 12, 1, 7);
	/*printf("Hello world from kernel space\n");*/
}
