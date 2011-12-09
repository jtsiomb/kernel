#ifndef FS_H_
#define FS_H_

#include <inttypes.h>

#define MAGIC	0xccf5ccf5
#define BLKSZ	1024

#define SECT_TO_BLK(x)	((x) / (BLKSZ / 512))

#define DEV_MAJOR(dev)	(((dev) >> 8) & 0xff)
#define DEV_MINOR(dev)	((dev) & 0xff)


typedef uint32_t dev_t;
typedef uint32_t blkid;

struct superblock {
	uint32_t magic;	/* magic number */
	int ver;		/* filesystem version */
	int blksize;	/* only BLKSZ supported at the moment */

	/* total number of blocks */
	unsigned int num_blocks;
	/* inode allocation bitmap start and count */
	blkid ibm_start;
	unsigned int ibm_count;
	/* inode table start and count */
	blkid itbl_start;
	unsigned int itbl_count;
	/* data block allocation bitmap start and count */
	blkid dbm_start;
	unsigned int dbm_count;
	/* data blocks start and count */
	blkid data_start;
	unsigned int data_count;

	int root_ino;	/* root direcotry inode */

	/* the following are valid only at runtime, ignored on disk */
	uint32_t *ibm;	/* memory inode bitmap */
	uint32_t *dbm;	/* memory datablock bitmap */

} __attribute__((packed));


/* 20 direct blocks + 10 attributes + 2 indirect = 128 bytes per inode */
#define NDIRBLK	20
struct inode {
	int ino;
	int uid, gid, mode;
	int nlink;
	dev_t dev;
	uint32_t atime, ctime, mtime;
	uint32_t size;
	blkid blk[NDIRBLK];	/* direct blocks */
	blkid ind;			/* indirect */
	blkid dind;			/* double-indirect */
} __attribute__((packed));


int sys_mount(char *mntpt, char *devname, unsigned int flags);
int sys_umount(char *devname);

int sys_open(char *pathname, int flags, unsigned int mode);
int sys_close(int fd);

int sys_read(int fd, void *buf, int sz);
int sys_write(int fd, void *buf, int sz);
long sys_lseek(int fd, long offs, int from);

int lookup_path(const char *path);


#endif	/* FS_H_ */
