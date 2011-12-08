#ifndef PART_H_
#define PART_H_

#include <inttypes.h>

struct partition {
	uint32_t start_sect;
	size_t size_sect;

	unsigned int attr;

	struct partition *next;
};

struct partition *get_part_list(int devno);
void free_part_list(struct partition *plist);

int get_part_type(struct partition *p);

#endif	/* PART_H_ */
