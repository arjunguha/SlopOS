#ifndef SLOPOS_FLOPPY_H
#define SLOPOS_FLOPPY_H

int floppy_write_sector_lba(unsigned int lba, const unsigned char *data);

#endif
