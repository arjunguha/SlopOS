#ifndef SLOPOS_BOOT_H
#define SLOPOS_BOOT_H

#define BOOT_INFO_ADDR 0x9000

typedef struct BootInfo {
    unsigned int ramdisk_base;
    unsigned int ramdisk_size;
    unsigned int ramdisk_lba;
    unsigned int e820_count;
    unsigned int e820_addr;
} BootInfo;

static inline const BootInfo *boot_info(void) {
    return (const BootInfo *)BOOT_INFO_ADDR;
}

#endif
