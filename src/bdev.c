#include <stdlib.h>
#include <assert.h>
#include "bdev.h"
#include "ata.h"
#include "part.h"

#define MINOR_DISK(x)	(((x) >> 4) & 0xf)
#define MINOR_PART(x)	((x) & 0xf)

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

		free_part_list(plist);
	} else {
		bdev->offset = 0;
		bdev->size = SECT_TO_BLK(ata_num_sectors(devno));
	}

	return bdev;
}

void blk_close(struct block_device *bdev)
{
	free(bdev);
}

#define NSECT	(BLKSZ / 512)

int blk_read(struct block_device *bdev, uint32_t blk, void *buf)
{
	int i;
	char *ptr = buf;
	uint32_t sect = blk * NSECT;

	for(i=0; i<NSECT; i++) {
		if(ata_read_pio(bdev->ata_dev, sect++, ptr) == -1) {
			return -1;
		}
		ptr += 512;
	}
	return 0;
}

int blk_write(struct block_device *bdev, uint32_t blk, void *buf)
{
	int i;
	char *ptr = buf;
	uint32_t sect = blk * NSECT;

	for(i=0; i<NSECT; i++) {
		if(ata_write_pio(bdev->ata_dev, sect++, ptr) == -1) {
			return -1;
		}
		ptr += 512;
	}
	return 0;
}
