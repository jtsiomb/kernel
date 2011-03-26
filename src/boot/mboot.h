#ifndef MBOOT_H_
#define MBOOT_H_

#include <inttypes.h>

#define MB_MEM		(1 << 0)
#define MB_BOOTDEV	(1 << 1)
#define MB_CMDLINE	(1 << 2)
#define MB_MODULES	(1 << 3)
#define MB_AOUT_SYM	(1 << 4)
#define MB_ELF_SHDR	(1 << 5)
#define MB_MMAP		(1 << 6)
#define MB_DRIVES	(1 << 7)
#define MB_CFGTAB	(1 << 8)
#define MB_LDRNAME	(1 << 9)
#define MB_APM		(1 << 10)
#define MB_GFX		(1 << 11)

#define MB_MEM_VALID	1
#define MB_DRIVE_CHS	0
#define MB_DRIVE_LBA	1

struct mboot_module {
	uint32_t start_addr, end_addr;
	char *str;
	uint32_t reserved;
};

struct mboot_elf_shdr_table {
	uint32_t num;
	uint32_t size;
	uint32_t addr;
	uint32_t shndx;
};

struct mboot_mmap {
	uint32_t skip;
	uint32_t base_low, base_high;
	uint32_t length_low, length_high;
	uint32_t type;
};

struct mboot_drive {
	uint32_t size;
	uint8_t id;
	uint8_t mode;
	uint16_t cyl;
	uint8_t heads, sect;
	uint16_t ports[1];	/* zero-terminated */
} __attribute__ ((packed));

struct mboot_apm {
	uint16_t ver;
	uint16_t cseg;
	uint32_t offs;
	uint16_t cseg16;
	uint16_t dseg;
	uint16_t flags;
	uint16_t cseg_len;
	uint16_t cseg16_len;
	uint16_t dseg_len;
} __attribute__ ((packed));

struct mboot_vbe {
	uint32_t ctl_info;
	uint32_t mode_info;
	uint16_t mode;
	uint16_t ifseg, ifoffs, iflen;
} __attribute__ ((packed));


/* multiboot information structure */
struct mboot_info {
	uint32_t flags;
	/* mem_lower: available low memory (up to 640kb)
	 * mem_upper: available upper memory (from 1mb and upwards)
	 */
	uint32_t mem_lower, mem_upper;
	/* boot device fields: MSB -> [part3|part2|part1|drive] <- LSB */
	uint32_t boot_dev;
	char *cmdline;
	/* loaded kernel modules */
	uint32_t mods_count;
	struct mboot_module *mods;
	/* elf sections table */
	struct mboot_elf_shdr_table elf;
	/* memory map */
	uint32_t mmap_len;
	struct mboot_mmap *mmap;
	/* drives table */
	uint32_t drives_len;
	struct mboot_drive *drives;
	/* address of BIOS ROM configuration table */
	uint32_t cfgtable;
	char *boot_loader_name;
	/* advanced power management */
	struct mboot_apm *apm;
	/* video bios extensions */
	struct mboot_vbe vbe;
};


#endif	/* MBOOT_H_ */
