#ifndef ATA_H_
#define ATA_H_

void init_ata(void);

int ata_read_pio(int devno, uint64_t sect, void *buf);
int ata_write_pio(int devno, uint64_t sect, void *buf);

#endif	/* ATA_H_ */
