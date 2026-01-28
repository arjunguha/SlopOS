#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 PATH/to/init.scm" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INIT_PATH="$1"
BUILD_DIR="$ROOT_DIR/build"
MKFS="$ROOT_DIR/scripts/mkfs.py"
QEMU="${QEMU:-qemu-system-i386}"

mkdir -p "$BUILD_DIR"
TMP_DIR="$(mktemp -d "$BUILD_DIR/tmp_test.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

cp "$ROOT_DIR/programs/boot.scm" "$TMP_DIR/boot.scm"
cp "$INIT_PATH" "$TMP_DIR/init.scm"
python3 "$MKFS" "$TMP_DIR" "$TMP_DIR/fs.img"

make -C "$ROOT_DIR" -s "$BUILD_DIR/kernel.bin" "$BUILD_DIR/stage1.bin" "$BUILD_DIR/stage2.bin"

kernel_bytes=$(stat -c%s "$BUILD_DIR/kernel.bin")
kernel_sectors=$(( (kernel_bytes + 511) / 512 ))
stage2_bytes=$(stat -c%s "$BUILD_DIR/stage2.bin")
stage2_sectors=$(( (stage2_bytes + 511) / 512 ))
ramdisk_bytes=$(stat -c%s "$TMP_DIR/fs.img")
ramdisk_sectors=$(( (ramdisk_bytes + 511) / 512 ))
kernel_lba=$(( 1 + stage2_sectors ))
ramdisk_lba=$(( kernel_lba + kernel_sectors ))
disk_sectors=2880
IMG="$TMP_DIR/os.img"

dd if=/dev/zero of="$IMG" bs=512 count=$disk_sectors status=none
dd if="$BUILD_DIR/stage1.bin" of="$IMG" bs=512 count=1 conv=notrunc status=none
dd if="$BUILD_DIR/stage2.bin" of="$IMG" bs=512 seek=1 conv=notrunc status=none
dd if="$BUILD_DIR/kernel.bin" of="$IMG" bs=512 seek=$kernel_lba conv=notrunc status=none
dd if="$TMP_DIR/fs.img" of="$IMG" bs=512 seek=$ramdisk_lba conv=notrunc status=none

exec "$QEMU" -drive if=floppy,format=raw,file="$IMG" -display none -serial stdio -monitor none
