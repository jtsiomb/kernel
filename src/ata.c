#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "ata.h"
#include "intr.h"
#include "asmops.h"

/* registers */
#define REG_DATA		0	/* R/W */
#define REG_ERROR		1	/*  R  */
#define REG_FEATURES	1	/*  W  */
#define REG_COUNT		2	/* R/W */
#define REG_LBA0		3	/* R/W */
#define REG_LBA1		4	/* R/W */
#define REG_LBA2		5	/* R/W */
#define REG_DEVICE		6	/* R/W */
#define REG_CMD			7	/*  W  */
#define REG_STATUS		7	/*  R  */

#define REG_CTL			518
#define REG_ALTSTAT		518

/* status bit fields */
#define ST_ERR		(1 << 0)
#define ST_DRQ		(1 << 3)
#define ST_DRDY		(1 << 6)
#define ST_BSY		(1 << 7)

/* device select bit in control register */
#define DEV_SEL(x)		(((x) & 1) << 4)

/* ATA commands */
#define CMD_IDENTIFY	0xec


struct device {
	int id;		/* id of the device on its ATA interface (0 master, 1 slave) */
	int iface;	/* ATA interface for this device (0 or 1) */
	int port_base;	/* interface I/O port base */

	uint32_t nsect_lba;
	uint64_t nsect_lba48;
};


static int identify(struct device *dev, int iface, int id);
static void select_dev(struct device *dev);
static int wait_busy(struct device *dev);
static int wait_drq(struct device *dev);
static void read_data(struct device *dev, void *buf);
static inline uint8_t read_reg8(struct device *dev, int reg);
static inline uint16_t read_reg16(struct device *dev, int reg);
static inline void write_reg8(struct device *dev, int reg, uint8_t val);
static inline void write_reg16(struct device *dev, int reg, uint16_t val);
static void ata_intr(int inum);
static void *atastr(void *res, void *src, int n);
static char *size_str(uint64_t nsect, char *buf);

/* last drive selected on each bus */
static int drvsel[2] = {-1, -1};

/* 4 possible devices: 2 ATA interfaces with 2 devices each.
 * this will never change unless we start querying the PCI config space
 * for additional drives (in which case this whole init code must be
 * rewritten anyway), but I like it spelt out like this.
 */
#define MAX_IFACES		2
#define MAX_DEV			(MAX_IFACES * 2)
static struct device dev[MAX_DEV];


void init_ata(void)
{
	int i;

	interrupt(IRQ_TO_INTR(15), ata_intr);

	for(i=0; i<MAX_DEV; i++) {
		int iface = i / MAX_IFACES;
		int id = i % MAX_IFACES;

		if(identify(dev + i, iface, id) == -1) {
			dev[i].id = -1;
		}
	}
}

static int identify(struct device *dev, int iface, int id)
{
	/* base address of the two ATA interfaces */
	static const int port_base[] = {0x1f0, 0x170};
	unsigned char st;
	uint16_t *info;
	char textbuf[42];	/* at most we need 40 chars for ident strings */

	dev->id = id;
	dev->iface = iface;
	dev->port_base = port_base[iface];

	/* a status of 0xff means there's no drive on the interface */
	if((st = read_reg8(dev, REG_ALTSTAT)) == 0xff) {
		return -1;
	}

	select_dev(dev);
	/* wait a bit to allow the device time to respond */
	iodelay(); iodelay(); iodelay(); iodelay();

	write_reg8(dev, REG_CMD, CMD_IDENTIFY);

	if(!(st = read_reg8(dev, REG_ALTSTAT)) || (st & ST_ERR)) {
		/* does not exist */
		return -1;
	}
	if(wait_busy(dev) == -1) {
		/* got ST_ERR, not ATA */
		return -1;
	}

	info = malloc(512);
	assert(info);

	/* read the device information */
	read_data(dev, info);

	/* print model and serial */
	printf("- found ata drive (%d,%d): %s\n", dev->iface, dev->id, atastr(textbuf, info + 27, 40));
	printf("  s/n: %s\n", atastr(textbuf, info + 10, 20));

	dev->nsect_lba = *(uint32_t*)(info + 60);
	dev->nsect_lba48 = *(uint64_t*)(info + 100) & 0xffffffffffffull;

	if(!dev->nsect_lba) {
		printf("  drive does not support LBA, ignoring!\n");
		free(info);
		return -1;
	}

	if(dev->nsect_lba48) {
		size_str(dev->nsect_lba48, textbuf);
	} else {
		size_str(dev->nsect_lba, textbuf);
	}
	printf("  size: %s\n", textbuf);

	free(info);
	return 0;
}

static void select_dev(struct device *dev)
{
	/* if this is the currently selected device, thy work is done */
	if(drvsel[dev->iface] == dev->id)
		return;

	/* wait for BSY and DRQ to clear */
	while(read_reg8(dev, REG_ALTSTAT) & (ST_BSY | ST_DRQ));

	/* set the correct device bit to the device register */
	write_reg8(dev, REG_DEVICE, DEV_SEL(dev->id));
}

static int wait_busy(struct device *dev)
{
	unsigned char st;

	do {
		st = read_reg8(dev, REG_ALTSTAT);
	} while((st & ST_BSY) && !(st & ST_ERR));

	return st & ST_ERR ? -1 : 0;
}

static int wait_drq(struct device *dev)
{
	unsigned char st;

	do {
		st = read_reg8(dev, REG_ALTSTAT);
	} while(!(st & (ST_DRQ | ST_ERR)));

	return st & ST_ERR ? -1 : 0;
}

static void read_data(struct device *dev, void *buf)
{
	int i;
	uint16_t *ptr = buf;

	/* wait for the data request from the drive */
	wait_drq(dev);

	/* ready to transfer */
	for(i=0; i<256; i++) {
		*ptr++ = read_reg16(dev, REG_DATA);
	}
}

static inline uint8_t read_reg8(struct device *dev, int reg)
{
	uint8_t val;
	inb(val, dev->port_base + reg);
	return val;
}

static inline uint16_t read_reg16(struct device *dev, int reg)
{
	uint16_t val;
	inw(val, dev->port_base + reg);
	return val;
}

static inline void write_reg8(struct device *dev, int reg, uint8_t val)
{
	outb(val, dev->port_base + reg);
}

static inline void write_reg16(struct device *dev, int reg, uint16_t val)
{
	outw(val, dev->port_base + reg);
}

static void ata_intr(int inum)
{
	printf("ATA interrupt\n");
}

static void *atastr(void *res, void *src, int n)
{
	int i;
	uint16_t *sptr = (uint16_t*)src;
	char *dptr = res;

	for(i=0; i<n/2; i++) {
		*dptr++ = (*sptr & 0xff00) >> 8;
		*dptr++ = *sptr++ & 0xff;
	}
	*dptr = 0;
	return res;
}

static char *size_str(uint64_t nsect, char *buf)
{
	static const char *suffix[] = {"kb", "mb", "gb", "tb", "pb", 0};
	int i;

	/* start with kilobytes */
	nsect /= 2;

	for(i=0; nsect >= 1024 && suffix[i + 1]; i++) {
		nsect /= 1024;
	}
	sprintf(buf, "%u %s", (unsigned int)nsect, suffix[i]);
	return buf;
}

