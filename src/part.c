#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "part.h"
#include "ata.h"

#define PTYPE_EXT		0x5
#define PTYPE_EXT_LBA	0xf

#define PATTR_ACT_BIT	(1 << 9)
#define PATTR_PRIM_BIT	(1 << 10)

#define PTYPE(attr)		((attr) & 0xff)
#define IS_ACT(attr)	((attr) & PATTR_ACT_BIT)
#define IS_PRIM(attr)	((attr) & PATTR_PRIM_BIT)

#define BOOTSIG_OFFS	510
#define PTABLE_OFFS		0x1be

#define BOOTSIG			0xaa55

#define IS_MBR			(sidx == 0)
#define IS_FIRST_EBR	(!IS_MBR && (first_ebr_offs == 0))

struct part_record {
	uint8_t stat;
	uint8_t first_head, first_cyl, first_sect;
	uint8_t type;
	uint8_t last_head, last_cyl, last_sect;
	uint32_t first_lba;
	uint32_t nsect_lba;
} __attribute__((packed));


static uint16_t bootsig(const char *sect);


struct partition *get_part_list(int devno)
{
	char *sect;
	struct partition *phead = 0, *ptail = 0;
	uint32_t sidx = 0;
	uint32_t first_ebr_offs = 0;
	int i, num_bootrec = 0;

	sect = malloc(512);
	assert(sect);

	do {
		int num_rec;
		struct part_record *prec;

		if(IS_FIRST_EBR) {
			first_ebr_offs = sidx;
		}

		if(ata_read_pio(devno, sidx, sect) == -1) {
			goto err;
		}
		if(bootsig(sect) != BOOTSIG) {
			printf("invalid/corrupted partition table, sector %lu has no magic\n", (unsigned long)sidx);
			goto err;
		}
		prec = (struct part_record*)(sect + PTABLE_OFFS);

		/* MBR has 4 records, EBRs have 2 */
		num_rec = IS_MBR ? 4 : 2;

		for(i=0; i<num_rec; i++) {
			struct partition *pnode;

			/* ignore empty partitions in the MBR, stop if encountered in an EBR */
			if(prec[i].type == 0) {
				if(num_bootrec > 0) {
					sidx = 0;
					break;
				}
				continue;
			}

			/* ignore extended partitions and setup sector index to read
			 * the next logical partition afterwards.
			 */
			if(prec[i].type == PTYPE_EXT || prec[i].type == PTYPE_EXT_LBA) {
				/* all EBR start fields are relative to the first EBR offset */
				sidx = first_ebr_offs + prec[i].first_lba;
				continue;
			}

			pnode = malloc(sizeof *pnode);
			assert(pnode);

			pnode->attr = prec[i].type;

			if(prec[i].stat & 0x80) {
				pnode->attr |= PATTR_ACT_BIT;
			}
			if(IS_MBR) {
				pnode->attr |= PATTR_PRIM_BIT;
			}
			pnode->start_sect = prec[i].first_lba + first_ebr_offs;
			pnode->size_sect = prec[i].nsect_lba;
			pnode->next = 0;

			/* append to the list */
			if(!phead) {
				phead = ptail = pnode;
			} else {
				ptail->next = pnode;
				ptail = pnode;
			}
		}

		num_bootrec++;
	} while(sidx > 0);

	free(sect);
	return phead;

err:
	free(sect);
	while(phead) {
		void *tmp = phead;
		phead = phead->next;
		free(tmp);
	}
	return 0;
}

void free_part_list(struct partition *plist)
{
	while(plist) {
		struct partition *tmp = plist;
		plist = plist->next;
		free(tmp);
	}
}

int get_part_type(struct partition *p)
{
	return PTYPE(p->attr);
}


static uint16_t bootsig(const char *sect)
{
	return *(uint16_t*)(sect + BOOTSIG_OFFS);
}

