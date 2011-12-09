#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/fs.h>
#endif
#ifdef __darwin__
#include <dev/disk.h>
#endif
#include "fs.h"

int mkfs(int fd, int blksize, uint32_t nblocks);
uint32_t get_block_count(int fd, int blksize);
int user_readblock(int dev, uint32_t blk, void *buf);
int user_writeblock(int dev, uint32_t blk, void *buf);
int parse_args(int argc, char **argv);

int fd;
uint32_t num_blocks;

int main(int argc, char **argv)
{
	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if((num_blocks = get_block_count(fd, BLKSZ)) == 0) {
		fprintf(stderr, "could not determine the number of blocks\n");
		return 1;
	}
	printf("total blocks: %u\n", (unsigned int)num_blocks);

	if(mkfs(fd, num_blocks) == -1) {
		return 1;
	}

	return 0;
}

int mkfs(int fd, int blksize, uint32_t nblocks)
{
	struct superblock *sb;

	if(!(sb = malloc(BLKSZ))) {
		perror("failed to allocate memory");
		return -1;
	}
}

uint32_t get_block_count(int fd, int blksize)
{
	unsigned long sz = 0;
	uint64_t sz64 = 0;
	struct stat st;

#ifdef BLKGETSIZE64
	if(ioctl(fd, BLKGETSIZE64, &sz64) != -1) {
		return sz64 / blksize;
	}
#endif

#ifdef BLKGETSIZE
	if(ioctl(fd, BLKGETSIZE, &sz) != -1) {
		return sz / (blksize / 512);
	}
#endif

#ifdef DKIOCGETBLOCKCOUNT
	if(ioctl(fd, DKIOCGETBLOCKCOUNT, &sz64) != -1) {
		return sz64 / (blksize / 512);
	}
#endif

	if(fstat(fd, &st) != -1 && S_ISREG(st.st_mode)) {
		return st.st_size / blksize;
	}

	return 0;
}

int user_readblock(int dev, uint32_t blk, void *buf)
{
	if(lseek(fd, blk * BLKSZ, SEEK_SET) == -1) {
		return -1;
	}
	if(read(fd, buf, BLKSZ) < BLKSZ) {
		return -1;
	}
	return 0;
}

int user_writeblock(int dev, uint32_t blk, void *buf)
{
	if(lseek(fd, blk * BLKSZ, SEEK_SET) == -1) {
		return -1;
	}
	if(write(fd, buf, BLKSZ) < BLKSZ) {
		return -1;
	}
	return 0;
}

int parse_args(int argc, char **argv)
{
	int i;

	fd = -1;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-' && argv[i][2] == 0) {
			switch(argv[i][1]) {
			case 'h':
				printf("usage: %s <device file>\n", argv[0]);
				exit(0);

			default:
				goto invalid;
			}
		} else {
			if(fd != -1) {
				goto invalid;
			}

			if((fd = open(argv[i], O_RDWR)) == -1) {
				fprintf(stderr, "failed to open %s: %s\n", argv[i], strerror(errno));
				return -1;
			}
		}
	}

	if(fd == -1) {
		fprintf(stderr, "you must specify a device or image file\n");
		return -1;
	}

	return 0;

invalid:
	fprintf(stderr, "invalid argument: %s\n", argv[i]);
	return -1;
}
