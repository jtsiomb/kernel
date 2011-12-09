#ifndef BDEV_H_
#define BDEV_H_

#include "fs.h"	/* for dev_t */

/* TODO buffer cache */

struct block_device {
	int ata_dev;
	uint32_t offset, size;
};

struct block_device *blk_open(dev_t dev);
void blk_close(struct block_device *bdev);

int blk_read(struct block_device *bdev, uint32_t blk, void *buf);
int blk_write(struct block_device *bdev, uint32_t blk, void *buf);

dev_t bdev_by_name(const char *name);

#endif	/* BDEV_H_ */
