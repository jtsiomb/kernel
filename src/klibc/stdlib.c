#include <stdlib.h>
#include <ctype.h>

int atoi(const char *str)
{
	return atol(str);
}

long atol(const char *str)
{
	long acc = 0;
	int sign = 1;

	while(isspace(*str)) str++;

	if(*str == '+') {
		str++;
	} else if(*str == '-') {
		sign = -1;
		str++;
	}

	while(*str && isdigit(*str)) {
		acc = acc * 10 + (*str - '0');
		str++;
	}

	return sign > 0 ? acc : -acc;
}

void itoa(int val, char *buf, int base)
{
	static char rbuf[16];
	char *ptr = rbuf;
	int neg = 0;

	if(val < 0) {
		neg = 1;
		val = -val;
	}

	if(val == 0) {
		*ptr++ = '0';
	}

	while(val) {
		int digit = val % base;
		*ptr++ = digit < 10 ? (digit + '0') : (digit - 10 + 'a');
		val /= base;
	}

	if(neg) {
		*ptr++ = '-';
	}

	ptr--;

	while(ptr >= rbuf) {
		*buf++ = *ptr--;
	}
	*buf = 0;
}

void utoa(unsigned int val, char *buf, int base)
{
	static char rbuf[16];
	char *ptr = rbuf;

	if(val == 0) {
		*ptr++ = '0';
	}

	while(val) {
		unsigned int digit = val % base;
		*ptr++ = digit < 10 ? (digit + '0') : (digit - 10 + 'a');
		val /= base;
	}

	ptr--;

	while(ptr >= rbuf) {
		*buf++ = *ptr--;
	}
	*buf = 0;
}

