#include "ata.h"
#include "ports.h"

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

static void ata_io_delay(void) {
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

static int ata_wait_not_busy(void) {
    unsigned char status;
    for (unsigned int i = 0; i < 1000000; i++) {
        status = inb(ATA_STATUS);
        if ((status & ATA_SR_BSY) == 0) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(void) {
    unsigned char status;
    for (unsigned int i = 0; i < 1000000; i++) {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) {
            return -1;
        }
        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
    return -1;
}

int ata_write_sector_lba(unsigned int lba, const unsigned char *data) {
    if (ata_wait_not_busy() < 0) {
        return -1;
    }
    outb(ATA_DRIVE, (unsigned char)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_io_delay();
    if (ata_wait_not_busy() < 0) {
        return -1;
    }
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, (unsigned char)(lba & 0xFF));
    outb(ATA_LBA_MID, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, 0x30);

    if (ata_wait_drq() < 0) {
        return -1;
    }

    const unsigned short *words = (const unsigned short *)data;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, words[i]);
    }

    if (ata_wait_not_busy() < 0) {
        return -1;
    }

    return 0;
}
