ASM_STAGE1 := src/stage1.asm
ASM_STAGE2 := src/stage2.asm
BUILD := build
STAGE1 := $(BUILD)/stage1.bin
STAGE2 := $(BUILD)/stage2.bin
IMG := $(BUILD)/os.img

KERNEL_ASM := src/kernel/entry.asm
KERNEL_C := src/kernel/kernel.c src/kernel/console.c src/scheme/scheme.c
KERNEL_OBJS := $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/console.o $(BUILD)/scheme.o
KERNEL_ELF := $(BUILD)/kernel.elf
KERNEL_BIN := $(BUILD)/kernel.bin

NASM ?= nasm
QEMU ?= qemu-system-i386
CROSS ?= i386-elf-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS := -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -m32 -O2 -Wall -Wextra -nostdlib -nostdinc -DSCHEME_NO_STDLIB -I src/kernel -I src
LDFLAGS := -T linker.ld

.PHONY: all run clean

all: $(IMG)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: $(KERNEL_ASM) | $(BUILD)
	$(NASM) -f elf32 -o $@ $<

$(BUILD)/kernel.o: src/kernel/kernel.c src/kernel/console.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/console.o: src/kernel/console.c src/kernel/console.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/scheme.o: src/scheme/scheme.c src/scheme/scheme.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERNEL_ELF): $(KERNEL_OBJS) linker.ld | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $< $@

$(STAGE2): $(ASM_STAGE2) $(KERNEL_BIN) | $(BUILD)
	@kernel_bytes=`stat -c%s $(KERNEL_BIN)`; \
	kernel_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$kernel_bytes`; \
	$(NASM) -f bin -DKERNEL_SECTORS=1 -DKERNEL_LBA=1 -o $@.tmp $(ASM_STAGE2); \
	stage2_bytes=`stat -c%s $@.tmp`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	kernel_lba=`python3 -c "import sys; print(1 + int(sys.argv[1]))" $$stage2_sectors`; \
	$(NASM) -f bin -DKERNEL_SECTORS=$$kernel_sectors -DKERNEL_LBA=$$kernel_lba -o $@ $(ASM_STAGE2); \
	rm -f $@.tmp

$(STAGE1): $(ASM_STAGE1) $(STAGE2) | $(BUILD)
	@stage2_bytes=`stat -c%s $(STAGE2)`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	$(NASM) -f bin -DSTAGE2_SECTORS=$$stage2_sectors -o $@ $(ASM_STAGE1)

$(IMG): $(STAGE1) $(STAGE2) $(KERNEL_BIN) | $(BUILD)
	@stage2_bytes=`stat -c%s $(STAGE2)`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	kernel_lba=`python3 -c "import sys; print(1 + int(sys.argv[1]))" $$stage2_sectors`; \
	disk_sectors=`python3 -c "import sys; print(2880)"`; \
	dd if=/dev/zero of=$@ bs=512 count=$$disk_sectors; \
	dd if=$(STAGE1) of=$@ bs=512 count=1 conv=notrunc; \
	dd if=$(STAGE2) of=$@ bs=512 seek=1 conv=notrunc; \
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=$$kernel_lba conv=notrunc

run: $(IMG)
	$(QEMU) -drive format=raw,file=$(IMG) -display none -serial stdio -monitor none

scheme-host: src/scheme_host/main.c src/scheme/scheme.c src/scheme/scheme.h | $(BUILD)
	gcc -O2 -Wall -Wextra -I src -o $(BUILD)/scheme-host src/scheme_host/main.c src/scheme/scheme.c

clean:
	rm -rf $(BUILD)
