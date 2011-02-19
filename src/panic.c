#include <stdio.h>
#include <stdarg.h>
#include "asmops.h"

void panic(const char *fmt, ...)
{
	va_list ap;

	printf("~~~~~ kernel panic ~~~~~\n");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	disable_intr();
	halt_cpu();
}
