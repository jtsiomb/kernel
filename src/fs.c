/* This code is used by the kernel AND by userspace filesystem-related tools. */

/* XXX convention:
 * - functions that accept or return a struct inode, do not read/write it to disk
 * - functions that accept or return an int ino, do read/write it to disk
 * other kinds of blocks (data, indirect, etc) always hit the disk directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "fs.h"
#include "bdev.h"
#include "kdef.h"

/* number of inodes in a block */
#define BLK_INODES		(BLKSZ / sizeof(struct inode))
/* number of directory entries in a block */
#define BLK_DIRENT		(BLKSZ / sizeof(struct dir_entry))

#define BLKBITS				(BLKSZ * 8)

#define BM_IDX(x)			((x) / 32)
#define BM_BIT(x)			((x) & 0x1f)

#define BM_ISFREE(bm, x)	(((bm)[BM_IDX(x)] & (1 << BM_BIT(x))) == 0)
#define BM_SET(bm, x)		((bm)[BM_IDX(x)] |= (1 << BM_BIT(x)))
#define BM_CLR(bm, x)		((bm)[BM_IDX(x)] &= ~(1 << BM_BIT(x)))


static struct inode *newdir(struct filesys *fs, struct inode *parent);
static int addlink(struct filesys *fs, struct inode *target, struct inode *node, const char *name);
static int read_superblock(struct filesys *fs);
static int write_superblock(struct filesys *fs);
static int get_inode(struct filesys *fs, int ino, struct inode *inode);
static int put_inode(struct filesys *fs, struct inode *inode);
static int find_free(uint32_t *bm, int sz);
static int alloc_inode(struct filesys *fs);
#define free_inode(fs, ino)		BM_CLR((fs)->sb->ibm, (ino))
static int alloc_block(struct filesys *fs);
#define free_block(fs, bno)		BM_CLR((fs)->sb->bm, (bno))
#define zero_block(fs, bno) \
	do { \
		assert(bno > 0); \
		blk_write((fs)->bdev, (bno), 1, (fs)->zeroblock); \
	} while(0)

static int file_block(struct filesys *fs, struct inode *node, int boffs, int allocate);
#define get_file_block(fs, node, boffs)		file_block(fs, node, boffs, 0)
#define alloc_file_block(fs, node, boffs)	file_block(fs, node, boffs, 1)


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
		blk_close(bdev);
		return -ENOMEM;
	}
	if((res = read_superblock(fs)) != 0) {
		blk_close(bdev);
		return res;
	}

	/* allocate the zero-block buffer written to zero-out blocks */
	if(!(fs->zeroblock = malloc(fs->sb->blksize))) {
		blk_close(bdev);
		free(fs->sb->ibm);
		free(fs->sb->bm);
		free(fs->sb->root);
		return -ENOMEM;
	}
	memset(fs->zeroblock, 0xff, fs->sb->blksize);

	return 0;
}

int mkfs(struct filesys *fs, dev_t dev)
{
	struct superblock *sb;
	struct block_device *bdev;
	int i, bcount;

	if(!(bdev = blk_open(dev))) {
		return -1;
	}
	fs->bdev = bdev;

	if(!(sb = malloc(BLKSZ))) {
		blk_close(bdev);
		return -1;
	}
	fs->sb = sb;

	/* populate the superblock */
	sb->magic = MAGIC;
	sb->ver = FS_VER;
	sb->blksize = BLKSZ;

	sb->num_blocks = bdev->size;
	sb->num_inodes = sb->num_blocks / 4;

	/* inode bitmap just after the superblock */
	sb->ibm_start = 2;
	sb->ibm_count = (sb->num_inodes + BLKBITS - 1) / BLKBITS;
	/* also allocate and initialize in-memory inode bitmap */
	sb->ibm = malloc(sb->ibm_count * BLKSZ);
	assert(sb->ibm);
	memset(sb->ibm, 0, sb->ibm_count * BLKSZ);

	/* XXX mark inode 0 as used always */
	BM_SET(sb->ibm, 0);

	/* block bitmap just after the inode bitmap */
	sb->bm_start = sb->ibm_start + sb->ibm_count;
	sb->bm_count = (sb->num_blocks + BLKBITS - 1) / BLKBITS;
	/* also allocate and initialize in-memory block bitmap */
	sb->bm = malloc(sb->bm_count * BLKSZ);
	assert(sb->bm);
	memset(sb->bm, 0, sb->bm_count * BLKSZ);

	/* inode table, just after the block bitmap */
	sb->itbl_start = sb->bm_start + sb->bm_count;
	sb->itbl_count = (sb->num_inodes * sizeof(struct inode) + BLKSZ - 1) / BLKSZ;

	/* mark all used blocks as used */
	bcount = sb->itbl_start + sb->itbl_count;
	memset(sb->bm, 0xff, bcount / 8);
	for(i=0; i<bcount % 8; i++) {
		int bit = bcount / 8 + i;
		BM_SET(sb->bm, bit);
	}

	/* create the root directory */
	sb->root = newdir(fs, 0);
	sb->root_ino = sb->root->ino;
	/* and write the inode to disk */
	put_inode(fs, sb->root);

	return 0;
}

static struct inode *newdir(struct filesys *fs, struct inode *parent)
{
	struct inode *dirnode;

	/* allocate and initialize inode */
	if(!(dirnode = malloc(sizeof *dirnode))) {
		return 0;
	}
	memset(dirnode, 0, sizeof *dirnode);

	if((dirnode->ino = alloc_inode(fs)) == -1) {
		printf("failed to allocate inode for a new directory\n");
		free(dirnode);
		return 0;
	}
	dirnode->mode = S_IFDIR;

	/* add . and .. links */
	addlink(fs, dirnode, dirnode, ".");
	addlink(fs, dirnode, parent ? parent : dirnode, "..");

	return dirnode;
}

static int addlink(struct filesys *fs, struct inode *target, struct inode *node, const char *name)
{
	struct dir_entry ent, *data;
	int i, boffs, bidx, len;

	if(!(target->mode & S_IFDIR)) {
		return -ENOTDIR;
	}
	if(node->mode & S_IFDIR) {
		return -EPERM;
	}
	/* TODO check that the link does not already exist (EEXIST) */

	if((len = strlen(name)) > NAME_MAX) {
		return -ENAMETOOLONG;
	}
	ent.ino = node->ino;
	memcpy(ent.name, name, len + 1);

	/* find a place to put it */
	if(!(data = malloc(BLKSZ))) {
		return -ENOMEM;
	}

	boffs = 0;
	while((bidx = get_file_block(fs, target, boffs)) > 0) {
		/* read the block, and search for an empty entry */
		blk_read(fs->bdev, bidx, 1, data);

		/* for all directory entries in this block... */
		for(i=0; i<BLK_DIRENT; i++) {
			if(data[i].ino == 0) {
				/* found empty */
				memcpy(data + i, &ent, sizeof ent);
				goto success;
			}
		}
		boffs++;
	}

	/* didn't find any free entries amongst our blocks, allocate a new one */
	if(!(bidx = alloc_file_block(fs, target, boffs))) {
		free(data);
		return -ENOSPC;
	}
	/* zero-fill the new block and add the first entry */
	memset(data, 0, BLKSZ);
	*data = ent;

success:
	/* write to disk */
	blk_write(fs->bdev, bidx, 1, data);
	node->nlink++;	/* increase reference count */

	free(data);
	return 0;
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
	/* write the superblock itself */
	if(blk_write(fs->bdev, 1, 1, sb) == -1) {
		return -1;
	}
	return 0;
}

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

/* find a free element in the bitmap and return its number */
static int find_free(uint32_t *bm, int nbits)
{
	int i, j, nwords = nbits / 32;
	uint32_t ent = 0;

	for(i=0; i<=nwords; i++) {
		if(bm[i] != 0xffffffff) {
			for(j=0; j<32; j++) {
				if(BM_ISFREE(bm, ent)) {
					return ent;
				}
				ent++;
			}

			panic("shouldn't happen (in find_free:fs.c)");
		} else {
			ent += 32;
		}
	}

	return -1;
}

static int alloc_inode(struct filesys *fs)
{
	int ino;

	if((ino = find_free(fs->sb->ibm, fs->sb->num_inodes)) == -1) {
		return -1;
	}
	BM_SET(fs->sb->ibm, ino);
	return 0;
}

static int alloc_block(struct filesys *fs)
{
	int bno;

	if((bno = find_free(fs->sb->bm, fs->sb->num_blocks)) == -1) {
		return -1;
	}
	BM_SET(fs->sb->bm, bno);
	return 0;
}

#define BLK_BLKID	(BLKSZ / sizeof(blkid))
#define MAX_IND		(NDIRBLK + BLK_BLKID)
#define MAX_DIND	(MAX_IND + BLK_BLKID * BLK_BLKID)

static int file_block(struct filesys *fs, struct inode *node, int boffs, int allocate)
{
	int res, idx, node_dirty = 0;
	blkid *barr;

	/* out of bounds */
	if(boffs < 0 || boffs >= MAX_DIND) {
		return 0;
	}

	/* is it a direct block ? */
	if(boffs < NDIRBLK) {
		if(!(res = node->blk[boffs]) && allocate) {
			res = node->blk[boffs] = alloc_block(fs);
			if(res) {
				zero_block(fs, res);
				/* also write back the modified inode */
				put_inode(fs, node);
			}
		}
		return res;
	}

	barr = malloc(fs->sb->blksize);
	assert(barr);

	/* is it an indirect block ? */
	if(boffs < MAX_IND) {
		int ind_dirty = 0;

		if(node->ind) {
			/* read the indirect block */
			blk_read(fs->bdev, node->ind, 1, barr);
		} else {
			/* does not exist... try to allocate if requested */
			if(!allocate || !(node->ind = alloc_block(fs))) {
				res = 0;
				goto end;
			}

			/* allocated a block clear the buffer, and invalidate everything */
			memset(barr, 0, sizeof fs->sb->blksize);
			node_dirty = 1;
			ind_dirty = 1;
		}

		idx = boffs - NDIRBLK;

		if(!(res = barr[idx])) {
			if(allocate && (res = barr[idx] = alloc_block(fs))) {
				ind_dirty = 1;
			}
		}

		/* write back the indirect block if needed */
		if(ind_dirty) {
			blk_write(fs->bdev, node->ind, 1, barr);
		}
		goto end;
	}

	/* TODO check/rewrite this */
#if 0
	/* is it a double-indirect block ? */
	if(boffs < MAX_DIND) {
		/* first read the dind block and find the index of the ind block */
		if(!node->dind) {
			if(allocate) {
				/* allocate and zero-out the double indirect block */
				res = node->dind = alloc_block(fs);
				if(res) {
					zero_block(fs, res);
				}
			} else {
				res = 0;
				goto end;
			}
		}
		blk_read(fd->bdev, node->dind, 1, barr);
		idx = (boffs - MAX_IND) / BLK_BLKID;

		/* then read the ind block and find the index of the block */
		if(!barr[idx]) {
			res = 0;
			goto end;
		}
		blk_read(fd->bdev, barr[idx], 1, barr);
		res = barr[(boffs - MAX_IND) % BLK_BLKID];
	}
#endif

end:
	if(node_dirty) {
		put_inode(fs, node);
	}
	free(barr);
	return res;
}
