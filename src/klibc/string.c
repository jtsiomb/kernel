#include <string.h>

void memset(void *s, int c, size_t sz)
{
	int i;
	char *ptr = s;

	for(i=0; i<sz; i++) {
		*ptr++ = c;
	}
}
