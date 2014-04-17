/* implementation of the filesystem-related syscalls */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "fs.h"
#include "part.h"
#include "panic.h"
#include "bdev.h"

static dev_t find_rootfs(void);

/* list of mounted filesystems
 * XXX currently only one, the root filesystem
 */
static struct filesys *fslist;


int sys_mount(char *mtpt, char *devname, unsigned int flags)
{
	dev_t dev;
	int err;
	struct filesys *fs;

	if(strcmp(mtpt, "/") != 0) {
		printf("only root can be mounted at the moment\n");
		return -EBUG;
	}

	/* mounting root filesystem */
	if(fslist) {
		printf("root already mounted\n");
		return -EBUSY;
	}

	if(devname) {
		dev = bdev_by_name(devname);
	} else {
		/* try to autodetect it */
		dev = find_rootfs();
	}
	if(!dev) {
		err = -ENOENT;
		goto rootfail;
	}

	if(!(fs = malloc(sizeof *fslist))) {
		err = -ENOMEM;
		goto rootfail;
	}
	if((err = openfs(fs, dev)) != 0) {
		free(fs);
		goto rootfail;
	}

	fslist = fs;
	return 0;

rootfail:
	panic("failed to mount root filesystem: %d\n", -err);
	return err;	/* unreachable */
}

#define PART_TYPE	0xcc
static dev_t find_rootfs(void)
{
	dev_t dev = 0;
	int i, num_dev, partid;
	struct partition *plist, *p;
	struct superblock *sb = malloc(BLKSZ);
	char name[16];

	assert(sb);

	num_dev = ata_num_devices();
	for(i=0; i<num_dev; i++) {
		plist = p = get_part_list(i);

		partid = 0;
		while(p) {
			if(get_part_type(p) == PART_TYPE) {
				/* found the correct partition, now read the superblock
				 * and make sure it's got the correct magic id
				 */
				blk_read(i, p->start_sect / 2 + 1, BLKSZ, sb);

				if(sb->magic == MAGIC) {
					sprintf(name, "ata%dp%d", i, partid);
					printf("found root: %s\n", name);
					dev = bdev_by_name(name);
					break;
				}
			}
			p = p->next;
			partid++;
		}
		free_part_list(plist);
		if(dev) break;
	}

	free(sb);
	return dev;
}
