/* This code is used by the kernel AND by userspace filesystem-related tools.
 * The kernel-specific parts are conditionally compiled in #ifdef KERNEL blocks
 * the rest of the code should be independent.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "fs.h"
#include "part.h"

#ifdef KERNEL
#include "ata.h"
#include "panic.h"
#endif

struct filesys {
	int dev;
	struct partition part;

	struct superblock *sb;

	struct filesys *next;
};

static int find_rootfs(struct filesys *fs);
static int readblock(int dev, uint32_t blk, void *buf);
static int writeblock(int dev, uint32_t blk, void *buf);

/* root device & partition */
static struct filesys *fslist;

int sys_mount(char *mtpt, char *devname, unsigned int flags)
{
	if(strcmp(mtpt, "/") != 0) {
		printf("mount: only root can be mounted at the moment\n");
		return -EBUG;
	}

	/* mounting root filesystem */
	if(fslist) {

}

void init_fs(void)
{
	root.sb = malloc(512);
	assert(root.sb);

#ifdef KERNEL
	if(find_rootfs(&root) == -1) {
		panic("can't find root filesystem\n");
	}
#endif
}


#ifdef KERNEL
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
				readblock(i, p->start_sect / 2 + 1, fs->sb);

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

#define NSECT	(BLKSZ / 512)

static int readblock(struct block_device *bdev, uint32_t blk, void *buf)
{
	return blk_read(bdev, blk, buf);
}

static int writeblock(struct block_device *bdev, uint32_t blk, void *buf)
{
	return blk_write(bdev, blk, buf);
}
#else

/* if this is compiled as part of the user-space tools instead of the kernel
 * forward the call to a user read/write block function supplied by the app.
 */
int user_readblock(uint32_t, void*);
int user_writeblock(uint32_t, void*);

static int readblock(struct block_device *bdev, uint32_t blk, void *buf)
{
	return user_readblock(blk, buf);
}

static int writeblock(struct block_device *bdev, uint32_t blk, void *buf)
{
	return user_writeblock(blk, buf);
}
#endif	/* KERNEL */
