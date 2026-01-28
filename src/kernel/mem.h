#ifndef SLOPOS_MEM_H
#define SLOPOS_MEM_H

#include "boot.h"

void mem_init(const BootInfo *info, unsigned int kernel_end);
void *kmalloc(unsigned int size);
void *kmalloc_zero(unsigned int size);

#endif
