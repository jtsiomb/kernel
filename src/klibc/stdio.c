#include <stdio.h>

/* putchar is defined in term.c */

int puts(const char *s)
{
	while(*s) {
		putchar(*s++);
	}
	putchar('\n');
	return 0;
}
