#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* ATA PIO: controlador primario (0x1F0) — solo master */

void ata_init(void);
int  ata_present(void);

void ata_read_sector (uint32_t lba, void*       buf);  /* 512 bytes */
void ata_write_sector(uint32_t lba, const void* buf);  /* 512 bytes */

#endif
