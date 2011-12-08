#include <stdio.h>
#include "fs.h"
#include "ata.h"
#include "part.h"
#include "panic.h"

#define PART_TYPE	0xcc

static int find_rootfs(int *dev, struct partition *part);

/* root device & partition */
static int rdev;
static struct partition rpart;

void init_fs(void)
{
	if(find_rootfs(&rdev, &rpart) == -1) {
		panic("can't find root filesystem\n");
	}
}

static int find_rootfs(int *dev, struct partition *part)
{
	int i, num_dev, partid;
	struct partition *plist, *p;

	num_dev = ata_num_devices();
	for(i=0; i<num_dev; i++) {
		plist = p = get_part_list(i);

		partid = 0;
		while(p) {
			if(get_part_type(p) == PART_TYPE) {
				/* found it! */
				printf("using ata%dp%d\n", i, partid);
				*dev = i;
				*part = *p;
				return 0;
			}
			p = p->next;
			partid++;
		}
		free_part_list(plist);
	}
	return -1;
}
