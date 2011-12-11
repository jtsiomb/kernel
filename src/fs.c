/* This code is used by the kernel AND by userspace filesystem-related tools.
 * The kernel-specific parts are conditionally compiled in #ifdef KERNEL blocks
 * the rest of the code should be independent.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "fs.h"
#include "bdev.h"


int openfs(struct filesys *fs, dev_t dev);
static int read_superblock(struct block_device *bdev, struct superblock *sb);


int openfs(struct filesys *fs, dev_t dev)
{
	int res;
	struct block_device *bdev;
	struct superblock *sb = 0;

	if(!(bdev = blk_open(dev))) {
		return -ENOENT;
	}

	/* read the superblock */
	if(!(sb = malloc(BLKSZ))) {
		res = -ENOMEM;
		goto done;
	}
	if((res = read_superblock(bdev, sb)) != 0) {
		goto done;
	}




done:
	blk_close(bdev);
	free(sb);
	return res;
}

static int read_superblock(struct block_device *bdev, struct superblock *sb)
{
	/* read superblock and verify */
	if(blk_read(bdev, 1, 1, sb) == -1) {
		printf("failed to read superblock\n");
		return -EIO;
	}
	if(sb->magic != MAGIC) {
		printf("invalid magic\n");
		return -EINVAL;
	}
	if(sb->ver > FS_VER) {
		printf("invalid version: %d\n", sb->ver);
		return -EINVAL;
	}
	if(sb->blksize != BLKSZ) {
		printf("invalid block size: %d\n", sb->blksize);
		return -EINVAL;
	}

	/* allocate and populate in-memory bitmaps */
	if(!(sb->ibm = malloc(sb->ibm_count * sb->blksize))) {
		return -ENOMEM;
	}
	if(blk_read(bdev, sb->ibm_start, sb->ibm_count, sb->ibm) == -1) {
		printf("failed to read inode bitmap\n");
		free(sb->ibm);
		return -EIO;
	}
	if(!(sb->bm = malloc(sb->bm_count * sb->blksize))) {
		free(sb->ibm);
		return -ENOMEM;
	}
	if(blk_read(bdev, sb->bm_start, sb->bm_count, sb->bm) == -1) {
		printf("failed to read block bitmap\n");
		free(sb->ibm);
		free(sb->bm);
		return -EIO;
	}

	return 0;
}
