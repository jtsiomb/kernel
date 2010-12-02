#ifndef STRING_H_
#define STRING_H_

#include <stdlib.h>

void memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);

#endif	/* STRING_H_ */
