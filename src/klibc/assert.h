#ifndef ASSERT_H_
#define ASSERT_H_

#include "panic.h"

#define assert(x) \
	if(!(x)) { \
		panic("Kernel assertion failed: " #x "\n"); \
	}

#endif	/* ASSERT_H_ */
