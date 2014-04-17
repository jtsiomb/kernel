#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "bdev.h"
#include "ata.h"
#include "part.h"

#define MKMINOR(disk, part)	((((disk) & 0xf) << 4) | ((part) & 0xf))
#define MINOR_DISK(x)		(((x) >> 4) & 0xf)
#define MINOR_PART(x)		((x) & 0xf)

struct block_device *blk_open(dev_t dev)
{
	struct block_device *bdev;
	int i, minor, devno, part;

	/* XXX for now ignore the major number as we only have ata devices */
	minor = DEV_MINOR(dev);
	devno = MINOR_DISK(minor);
	part = MINOR_PART(minor);

	bdev = malloc(sizeof *bdev);
	assert(bdev);

	bdev->ata_dev = devno;

	if(part) {
		struct partition *plist = get_part_list(devno);
		assert(plist);

		for(i=1; i<part; i++) {
			if(!plist) break;
			plist = plist->next;
		}
		if(!plist) {
			free(bdev);
			free_part_list(plist);
			return 0;
		}

		bdev->offset = SECT_TO_BLK(plist->start_sect);
		bdev->size = SECT_TO_BLK(plist->size_sect);
		bdev->ptype = get_part_type(plist);

		free_part_list(plist);
	} else {
		bdev->offset = 0;
		bdev->size = SECT_TO_BLK(ata_num_sectors(devno));
		bdev->ptype = 0;
	}

	return bdev;
}

void blk_close(struct block_device *bdev)
{
	free(bdev);
}

#define NSECT	(BLKSZ / 512)

int blk_read(struct block_device *bdev, uint32_t blk, int count, void *buf)
{
	int i;
	char *ptr = buf;
	uint32_t sect = blk * NSECT + bdev->offset;

	for(i=0; i<NSECT * count; i++) {
		if(ata_read_pio(bdev->ata_dev, sect++, ptr) == -1) {
			return -1;
		}
		ptr += 512;
	}
	return 0;
}

int blk_write(struct block_device *bdev, uint32_t blk, int count, void *buf)
{
	int i;
	char *ptr = buf;
	uint32_t sect = blk * NSECT + bdev->offset;

	for(i=0; i<NSECT * count; i++) {
		if(ata_write_pio(bdev->ata_dev, sect++, ptr) == -1) {
			return -1;
		}
		ptr += 512;
	}
	return 0;
}

dev_t bdev_by_name(const char *name)
{
	int minor;
	int atadev, part = 0;

	char *tmp = strrchr(name, '/');
	if(tmp) {
		name = tmp + 1;
	}

	if(strstr(name, "ata") != name) {
		return 0;
	}
	name += 3;

	atadev = strtol(name, &tmp, 10);
	if(tmp == name) {
		return 0;
	}
	name = tmp;

	if(*name++ == 'p') {
		part = strtol(name, &tmp, 10) + 1;
		if(tmp == name) {
			return 0;
		}
	}

	minor = MKMINOR(atadev, part);
	return DEVNO(1, minor);
}
