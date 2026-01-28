ASM_STAGE1 := src/stage1.asm
ASM_STAGE2 := src/stage2.asm
BUILD := build
STAGE1 := $(BUILD)/stage1.bin
STAGE2 := $(BUILD)/stage2.bin
IMG := $(BUILD)/os.img
FS_DIR := programs
FSIMG := $(BUILD)/fs.img
FSIMG_INIT := $(BUILD)/fs_init.img
INIT_DIR := init_scripts
DEFAULT_INIT := default
INIT_SCRIPTS := $(shell find $(INIT_DIR) -type f -name "*.scm" 2>/dev/null)
INIT_NAMES := $(basename $(notdir $(INIT_SCRIPTS)))
MKFS := scripts/mkfs.py

KERNEL_ASM := src/kernel/entry.asm src/kernel/isr.asm src/kernel/context.asm
KERNEL_C := src/kernel/kernel.c src/kernel/console.c src/kernel/idt.c src/kernel/thread.c src/scheme/scheme.c
KERNEL_OBJS := $(BUILD)/entry.o $(BUILD)/isr.o $(BUILD)/context.o $(BUILD)/kernel.o $(BUILD)/console.o $(BUILD)/idt.o $(BUILD)/thread.o $(BUILD)/scheme.o
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

.PHONY: all run run-echo clean

all: $(IMG)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: src/kernel/entry.asm | $(BUILD)
	$(NASM) -f elf32 -o $@ $<

$(BUILD)/isr.o: src/kernel/isr.asm | $(BUILD)
	$(NASM) -f elf32 -o $@ $<

$(BUILD)/context.o: src/kernel/context.asm | $(BUILD)
	$(NASM) -f elf32 -o $@ $<

$(BUILD)/kernel.o: src/kernel/kernel.c src/kernel/console.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/console.o: src/kernel/console.c src/kernel/console.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/idt.o: src/kernel/idt.c src/kernel/idt.h src/kernel/ports.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/thread.o: src/kernel/thread.c src/kernel/thread.h src/kernel/ports.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/scheme.o: src/scheme/scheme.c src/scheme/scheme.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERNEL_ELF): $(KERNEL_OBJS) linker.ld | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $< $@

$(FSIMG): $(MKFS) $(FS_DIR)/boot.scm $(INIT_DIR)/$(DEFAULT_INIT).scm | $(BUILD)
	@mkdir -p $(BUILD)/tmp_programs_default; \
	cp $(FS_DIR)/boot.scm $(BUILD)/tmp_programs_default/boot.scm; \
	cp $(INIT_DIR)/$(DEFAULT_INIT).scm $(BUILD)/tmp_programs_default/init.scm; \
	python3 $(MKFS) $(BUILD)/tmp_programs_default $@

$(STAGE2): $(ASM_STAGE2) $(KERNEL_BIN) $(FSIMG) | $(BUILD)
	@kernel_bytes=`stat -c%s $(KERNEL_BIN)`; \
	kernel_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$kernel_bytes`; \
	ramdisk_bytes=`stat -c%s $(FSIMG)`; \
	ramdisk_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$ramdisk_bytes`; \
	$(NASM) -f bin -DKERNEL_SECTORS=1 -DKERNEL_LBA=1 -DRAMDISK_SECTORS=1 -DRAMDISK_LBA=1 -o $@.tmp $(ASM_STAGE2); \
	stage2_bytes=`stat -c%s $@.tmp`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	kernel_lba=`python3 -c "import sys; print(1 + int(sys.argv[1]))" $$stage2_sectors`; \
	ramdisk_lba=`python3 -c "import sys; print(int(sys.argv[1]) + int(sys.argv[2]))" $$kernel_lba $$kernel_sectors`; \
	$(NASM) -f bin -DKERNEL_SECTORS=$$kernel_sectors -DKERNEL_LBA=$$kernel_lba -DRAMDISK_SECTORS=$$ramdisk_sectors -DRAMDISK_LBA=$$ramdisk_lba -o $@ $(ASM_STAGE2); \
	rm -f $@.tmp

$(STAGE1): $(ASM_STAGE1) $(STAGE2) | $(BUILD)
	@stage2_bytes=`stat -c%s $(STAGE2)`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	$(NASM) -f bin -DSTAGE2_SECTORS=$$stage2_sectors -o $@ $(ASM_STAGE1)

$(IMG): $(STAGE1) $(STAGE2) $(KERNEL_BIN) $(FSIMG) | $(BUILD)
	@stage2_bytes=`stat -c%s $(STAGE2)`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	kernel_lba=`python3 -c "import sys; print(1 + int(sys.argv[1]))" $$stage2_sectors`; \
	kernel_bytes=`stat -c%s $(KERNEL_BIN)`; \
	kernel_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$kernel_bytes`; \
	ramdisk_lba=`python3 -c "import sys; print(int(sys.argv[1]) + int(sys.argv[2]))" $$kernel_lba $$kernel_sectors`; \
	disk_sectors=`python3 -c "import sys; print(2880)"`; \
	dd if=/dev/zero of=$@ bs=512 count=$$disk_sectors; \
	dd if=$(STAGE1) of=$@ bs=512 count=1 conv=notrunc; \
	dd if=$(STAGE2) of=$@ bs=512 seek=1 conv=notrunc; \
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=$$kernel_lba conv=notrunc; \
	dd if=$(FSIMG) of=$@ bs=512 seek=$$ramdisk_lba conv=notrunc

run: $(IMG)
	$(QEMU) -drive if=floppy,format=raw,file=$(IMG) -display none -serial stdio -monitor none

run-echo: $(BUILD)/os_echo.img
	@python3 - <<-'PY' | $(QEMU) -drive if=floppy,format=raw,file=$(BUILD)/os_echo.img -display none -serial stdio -monitor none
	import sys
	import time
	time.sleep(0.2)
	sys.stdout.write("Slopcoder 2000\n")
	sys.stdout.flush()
	PY

$(BUILD)/fs_%.img: $(MKFS) $(FS_DIR)/boot.scm $(INIT_DIR)/%.scm | $(BUILD)
	@mkdir -p $(BUILD)/tmp_programs_$*; \
	cp $(FS_DIR)/boot.scm $(BUILD)/tmp_programs_$*/boot.scm; \
	cp $(INIT_DIR)/$*.scm $(BUILD)/tmp_programs_$*/init.scm; \
	python3 $(MKFS) $(BUILD)/tmp_programs_$* $@

$(BUILD)/os_%.img: $(STAGE1) $(STAGE2) $(KERNEL_BIN) $(BUILD)/fs_%.img | $(BUILD)
	@stage2_bytes=`stat -c%s $(STAGE2)`; \
	stage2_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$stage2_bytes`; \
	kernel_lba=`python3 -c "import sys; print(1 + int(sys.argv[1]))" $$stage2_sectors`; \
	kernel_bytes=`stat -c%s $(KERNEL_BIN)`; \
	kernel_sectors=`python3 -c "import sys; size=int(sys.argv[1]); print((size + 511) // 512)" $$kernel_bytes`; \
	ramdisk_lba=`python3 -c "import sys; print(int(sys.argv[1]) + int(sys.argv[2]))" $$kernel_lba $$kernel_sectors`; \
	disk_sectors=`python3 -c "import sys; print(2880)"`; \
	dd if=/dev/zero of=$@ bs=512 count=$$disk_sectors; \
	dd if=$(STAGE1) of=$@ bs=512 count=1 conv=notrunc; \
	dd if=$(STAGE2) of=$@ bs=512 seek=1 conv=notrunc; \
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=$$kernel_lba conv=notrunc; \
	dd if=$(BUILD)/fs_$*.img of=$@ bs=512 seek=$$ramdisk_lba conv=notrunc

images: $(addprefix $(BUILD)/os_,$(addsuffix .img,$(INIT_NAMES)))

run-init:
	@name="$(NAME)"; \
	if [ -z "$$name" ]; then \
		echo "error: NAME not set (example: make run-init NAME=hello)"; \
		exit 1; \
	fi; \
	$(MAKE) $(BUILD)/os_$$name.img; \
	$(QEMU) -drive if=floppy,format=raw,file=$(BUILD)/os_$$name.img -display none -serial stdio -monitor none

scheme-host: src/scheme_host/main.c src/scheme/scheme.c src/scheme/scheme.h | $(BUILD)
	gcc -O2 -Wall -Wextra -I src -o $(BUILD)/scheme-host src/scheme_host/main.c src/scheme/scheme.c

clean:
	rm -rf $(BUILD)
