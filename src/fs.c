/* This code is used by the kernel AND by userspace filesystem-related tools.
 * The kernel-specific parts are conditionally compiled in #ifdef KERNEL blocks
 * the rest of the code should be independent.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "fs.h"
#include "bdev.h"

#define BM_IDX(x)			((x) / 32)
#define BM_BIT(x)			((x) & 0x1f)

#define BM_ISFREE(bm, x)	(((bm)[BM_IDX(x)] & (1 << BM_BIT(x))) == 0)
#define BM_SET(bm, x)		((bm)[BM_IDX(x)] |= (1 << BM_BIT(x)))
#define BM_CLR(bm, x)		((bm)[BM_IDX(x)] &= ~(1 << BM_BIT(x)))


int openfs(struct filesys *fs, dev_t dev);
static int read_superblock(struct filesys *fs);
static int write_superblock(struct filesys *fs);
static int get_inode(struct filesys *fs, int ino, struct inode *inode);
static int put_inode(struct filesys *fs, struct inode *inode);

int openfs(struct filesys *fs, dev_t dev)
{
	int res;
	struct block_device *bdev;

	assert(BLKSZ % sizeof(struct inode) == 0);

	if(!(bdev = blk_open(dev))) {
		return -ENOENT;
	}
	fs->bdev = bdev;

	/* read the superblock */
	if(!(fs->sb = malloc(BLKSZ))) {
		res = -ENOMEM;
		goto done;
	}
	if((res = read_superblock(fs)) != 0) {
		goto done;
	}


done:
	blk_close(bdev);
	return res;
}

static int read_superblock(struct filesys *fs)
{
	struct superblock *sb = fs->sb;

	/* read superblock and verify */
	if(blk_read(fs->bdev, 1, 1, sb) == -1) {
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
	if(blk_read(fs->bdev, sb->ibm_start, sb->ibm_count, sb->ibm) == -1) {
		printf("failed to read inode bitmap\n");
		free(sb->ibm);
		return -EIO;
	}
	if(!(sb->bm = malloc(sb->bm_count * sb->blksize))) {
		free(sb->ibm);
		return -ENOMEM;
	}
	if(blk_read(fs->bdev, sb->bm_start, sb->bm_count, sb->bm) == -1) {
		printf("failed to read block bitmap\n");
		free(sb->ibm);
		free(sb->bm);
		return -EIO;
	}

	/* read the root inode */
	if(!(sb->root = malloc(sizeof *sb->root))) {
		free(sb->ibm);
		free(sb->bm);
		return -ENOMEM;
	}
	if(get_inode(fs, sb->root_ino, sb->root) == -1) {
		printf("failed to read root inode\n");
		return -1;
	}

	return 0;
}

static int write_superblock(struct filesys *fs)
{
	struct superblock *sb = fs->sb;

	/* write back any changes in the root inode */
	if(put_inode(fs, sb->root) == -1) {
		return -1;
	}
	/* write back the block bitmap */
	if(blk_write(fs->bdev, sb->bm_start, sb->bm_count, sb->bm) == -1) {
		return -1;
	}
	/* write back the inode bitmap */
	if(blk_write(fs->bdev, sb->ibm_start, sb->ibm_count, sb->ibm) == -1) {
		return -1;
	}
	return 0;
}

/* number of inodes in a block */
#define BLK_INODES		(BLKSZ / sizeof(struct inode))

/* copy the requested inode from the disk, into the buffer passed in the last arg */
static int get_inode(struct filesys *fs, int ino, struct inode *inode)
{
	struct inode *buf = malloc(BLKSZ);
	assert(buf);

	if(blk_read(fs->bdev, fs->sb->itbl_start + ino / BLK_INODES, 1, buf) == -1) {
		free(buf);
		return -1;
	}
	memcpy(inode, buf + ino % BLK_INODES, sizeof *inode);
	free(buf);
	return 0;
}

/* write the inode to the disk */
static int put_inode(struct filesys *fs, struct inode *inode)
{
	struct inode *buf = malloc(BLKSZ);
	assert(buf);

	if(blk_read(fs->bdev, fs->sb->itbl_start + inode->ino / BLK_INODES, 1, buf) == -1) {
		free(buf);
		return -1;
	}
	memcpy(buf + inode->ino % BLK_INODES, inode, sizeof *inode);

	if(blk_write(fs->bdev, fs->sb->itbl_start + inode->ino / BLK_INODES, 1, buf) == -1) {
		free(buf);
		return -1;
	}
	free(buf);
	return 0;
}

static int find_free(uint32_t *bm, int sz)
{
	int i;
	uint32_t ent;

	for(i=0; i<=sz/32; i++) {
		if(bm[i] != 0xffffffff) {
			ent = i * 32;
			for(j=0; j<32; j++) {
				if(BM_ISFREE(bm, ent)) {
					return ent;
				}
			}

			panic("shouldn't happen (in find_free:fs.c)");
		}
	}

	return -1;
}

static int alloc_inode(struct filesys *fs)
{
	int ino;

	if((ino = find_free(fs->ibm, fs->ibm_count)) == -1) {
		return -1;
	}
	BM_SET(fs->ibm, ino);
	return 0;
}
