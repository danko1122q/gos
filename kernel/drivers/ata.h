#ifndef DRV_ATA_H
#define DRV_ATA_H
#include "lib/types.h"

void ata_init(void);
int  ata_present(void);
u32  ata_sectors(void);
int  ata_read(u32 lba, u32 count, void *buf);
int  ata_write(u32 lba, u32 count, const void *buf);

#endif
