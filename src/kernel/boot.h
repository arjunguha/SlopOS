#ifndef SLOPOS_BOOT_H
#define SLOPOS_BOOT_H

#define BOOT_INFO_ADDR 0x9000

typedef struct BootInfo {
    unsigned int ramdisk_base;
    unsigned int ramdisk_size;
} BootInfo;

static inline const BootInfo *boot_info(void) {
    return (const BootInfo *)BOOT_INFO_ADDR;
}

#endif
