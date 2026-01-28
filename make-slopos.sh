#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 PATH/to/output_dir" >&2
  exit 1
fi

OUT_DIR="$1"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
MKFS="$ROOT_DIR/scripts/mkfs.py"

if ! command -v i386-elf-gcc >/dev/null 2>&1; then
  if [[ -x "$HOME/opt/cross/bin/i386-elf-gcc" ]]; then
    export PATH="$HOME/opt/cross/bin:$PATH"
  else
    echo "error: i386-elf-gcc not found in PATH" >&2
    exit 1
  fi
fi

mkdir -p "$BUILD_DIR"
mkdir -p "$OUT_DIR"
TMP_DIR="$BUILD_DIR/tmp_programs_shell"
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

cp "$ROOT_DIR/programs/"*.scm "$TMP_DIR/"
cp "$ROOT_DIR/init_scripts/shell.scm" "$TMP_DIR/init.scm"

FS_IMG="$OUT_DIR/fs_shell.img"
python3 "$MKFS" "$TMP_DIR" "$FS_IMG"

make -C "$ROOT_DIR" -s build/kernel.bin
rm -f "$BUILD_DIR/stage2.bin"
make -C "$ROOT_DIR" -s FSIMG="$FS_IMG" build/stage2.bin
make -C "$ROOT_DIR" -s FSIMG="$FS_IMG" build/stage1.bin

kernel_bytes=$(stat -c%s "$BUILD_DIR/kernel.bin")
kernel_sectors=$(( (kernel_bytes + 511) / 512 ))
stage2_bytes=$(stat -c%s "$BUILD_DIR/stage2.bin")
stage2_sectors=$(( (stage2_bytes + 511) / 512 ))
ramdisk_bytes=$(stat -c%s "$FS_IMG")
ramdisk_sectors=$(( (ramdisk_bytes + 511) / 512 ))
kernel_lba=$(( 1 + stage2_sectors ))
ramdisk_lba=$(( kernel_lba + kernel_sectors ))
disk_sectors=2880
BOOT_IMG="$OUT_DIR/image.img"

dd if=/dev/zero of="$BOOT_IMG" bs=512 count=$disk_sectors status=none
dd if="$BUILD_DIR/stage1.bin" of="$BOOT_IMG" bs=512 count=1 conv=notrunc status=none
dd if="$BUILD_DIR/stage2.bin" of="$BOOT_IMG" bs=512 seek=1 conv=notrunc status=none
dd if="$BUILD_DIR/kernel.bin" of="$BOOT_IMG" bs=512 seek=$kernel_lba conv=notrunc status=none
dd if="$FS_IMG" of="$BOOT_IMG" bs=512 seek=$ramdisk_lba conv=notrunc status=none

echo "Built $BOOT_IMG"
echo "Built $FS_IMG"
echo "Run (raw tty so Ctrl-D reaches the shell):"
echo "stty raw -echo; qemu-system-i386 -drive if=floppy,format=raw,file=$BOOT_IMG -drive if=ide,format=raw,file=$FS_IMG -display none -serial stdio -monitor none; stty sane"
