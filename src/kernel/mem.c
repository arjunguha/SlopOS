#include "mem.h"
#include "console.h"

typedef struct E820Entry {
    unsigned int addr_low;
    unsigned int addr_high;
    unsigned int len_low;
    unsigned int len_high;
    unsigned int type;
} E820Entry;

static unsigned int heap_base;
static unsigned int heap_end;
static unsigned int heap_curr;

static unsigned int align_up(unsigned int value, unsigned int align) {
    return (value + align - 1) & ~(align - 1);
}

static unsigned int max_u32(unsigned int a, unsigned int b) {
    return a > b ? a : b;
}

void mem_init(const BootInfo *info, unsigned int kernel_end) {
    heap_base = 0;
    heap_end = 0;
    heap_curr = 0;

    unsigned int ramdisk_end = info->ramdisk_base + info->ramdisk_size;
    unsigned int desired = max_u32(kernel_end, ramdisk_end);
    desired = max_u32(desired, 0x00100000);
    desired = align_up(desired, 16);

    const E820Entry *entries = (const E820Entry *)info->e820_addr;
    unsigned int count = info->e820_count;
    unsigned int best_start = 0;
    unsigned int best_end = 0;

    for (unsigned int i = 0; i < count; i++) {
        const E820Entry *e = &entries[i];
        if (e->type != 1 || e->addr_high != 0 || e->len_high != 0) {
            continue;
        }
        unsigned int start = e->addr_low;
        unsigned int end = start + e->len_low;
        if (start <= desired && desired < end) {
            heap_base = desired;
            heap_end = end;
            heap_curr = heap_base;
            return;
        }
        if (start >= 0x00100000 && end - start > best_end - best_start) {
            best_start = start;
            best_end = end;
        }
    }

    if (best_start && best_end > best_start) {
        heap_base = align_up(best_start, 16);
        heap_end = best_end;
        heap_curr = heap_base;
    }

    if (heap_base == 0 || heap_end <= heap_base) {
        console_write("mem_init: no usable heap region\n");
    }
}

void *kmalloc(unsigned int size) {
    if (size == 0) {
        return 0;
    }
    size = align_up(size, 16);
    if (heap_curr == 0 || heap_curr + size > heap_end) {
        console_write("kmalloc: out of memory\n");
        return 0;
    }
    unsigned int addr = heap_curr;
    heap_curr += size;
    return (void *)addr;
}

void *kmalloc_zero(unsigned int size) {
    unsigned char *p = (unsigned char *)kmalloc(size);
    if (!p) {
        return 0;
    }
    for (unsigned int i = 0; i < size; i++) {
        p[i] = 0;
    }
    return p;
}
