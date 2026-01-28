#ifndef SLOPOS_ATA_H
#define SLOPOS_ATA_H

int ata_write_sector_lba(unsigned int lba, const unsigned char *data);

#endif
