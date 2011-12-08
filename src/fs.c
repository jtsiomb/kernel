#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fs.h"
#include "ata.h"
#include "part.h"
#include "panic.h"

#define MAGIC	0xccf5ccf5
#define BLKSZ	1024

typedef uint32_t blkid;

struct superblock {
	uint32_t magic;

	blkid istart;
	unsigned int icount;

	blkid dstart;
	unsigned int dcount;
};

struct filesys {
	int dev;
	struct partition part;

	struct superblock *sb;
};

static int find_rootfs(struct filesys *fs);

/* root device & partition */
static struct filesys root;

void init_fs(void)
{
	root.sb = malloc(512);
	assert(root.sb);

	if(find_rootfs(&root) == -1) {
		panic("can't find root filesystem\n");
	}
}

#define PART_TYPE	0xcc
static int find_rootfs(struct filesys *fs)
{
	int i, num_dev, partid;
	struct partition *plist, *p;

	num_dev = ata_num_devices();
	for(i=0; i<num_dev; i++) {
		plist = p = get_part_list(i);

		partid = 0;
		while(p) {
			if(get_part_type(p) == PART_TYPE) {
				/* found the correct partition, now read the superblock
				 * and make sure it's got the correct magic id
				 */
				ata_read_pio(i, p->start_sect + 2, fs->sb);

				if(fs->sb->magic == MAGIC) {
					printf("found root ata%dp%d\n", i, partid);
					fs->dev = i;
					fs->part = *p;
					return 0;
				}
			}
			p = p->next;
			partid++;
		}
		free_part_list(plist);
	}
	return -1;
}
