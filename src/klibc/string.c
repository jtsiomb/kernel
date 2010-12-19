#include <string.h>

void memset(void *s, int c, size_t n)
{
	char *ptr = s;
	while(n--) {
		*ptr++ = c;
	}
}

/* Does the same thing as memset only with 16bit values.
 * n in this case is the number of values, not the number of bytes.
 */
void memset16(void *s, int c, size_t n)
{
	short *ptr = s;
	while(n--) {
		*ptr++ = c;
	}
}

void *memcpy(void *dest, const void *src, size_t n)
{
	char *dptr = dest;
	const char *sptr = src;

	while(n--) {
		*dptr++ = *sptr++;
	}
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	int i;
	char *dptr;
	const char *sptr;

	if(dest <= src) {
		/* forward copy */
		dptr = dest;
		sptr = src;
		for(i=0; i<n; i++) {
			*dptr++ = *sptr++;
		}
	} else {
		/* backwards copy */
		dptr = dest + n - 1;
		sptr = src + n - 1;
		for(i=0; i<n; i++) {
			*dptr-- = *sptr--;
		}
	}

	return dest;
}

size_t strlen(const char *s)
{
	size_t len = 0;
	while(*s++) len++;
	return len;
}

char *strchr(const char *s, int c)
{
	while(*s) {
		if(*s == c) {
			return s;
		}
		s++;
	}
	return 0;
}

char *strrchr(const char *s, int c)
{
	char *ptr = s;

	/* find the end */
	while(*ptr) ptr++;

	/* go back checking for c */
	while(*--ptr >= s) {
		if(*ptr == c) {
			return ptr;
		}
	}
	return 0;
}
