#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 PATH/to/init.scm" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INIT_PATH="$1"
INIT_NAME="$(basename "$INIT_PATH" .scm)"
BUILD_DIR="$ROOT_DIR/build"
MKFS="$ROOT_DIR/scripts/mkfs.py"
QEMU="${QEMU:-qemu-system-i386}"
SNAPSHOT_FLAG="-snapshot"
if [[ "${SLOPOS_NO_SNAPSHOT:-}" == "1" ]]; then
  SNAPSHOT_FLAG=""
fi

if ! command -v i386-elf-gcc >/dev/null 2>&1; then
  if [[ -x "$HOME/opt/cross/bin/i386-elf-gcc" ]]; then
    export PATH="$HOME/opt/cross/bin:$PATH"
  else
    echo "error: i386-elf-gcc not found in PATH" >&2
    exit 1
  fi
fi

mkdir -p "$BUILD_DIR"
TMP_DIR="$BUILD_DIR/test_${INIT_NAME}_programs"
mkdir -p "$TMP_DIR"

cp "$ROOT_DIR/programs/boot.scm" "$TMP_DIR/boot.scm"
cp "$ROOT_DIR/programs/fs.scm" "$TMP_DIR/fs.scm"
cp "$INIT_PATH" "$TMP_DIR/init.scm"
FS_IMG="$BUILD_DIR/test_${INIT_NAME}_fs.img"
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
IMG="$BUILD_DIR/test_${INIT_NAME}.img"

dd if=/dev/zero of="$IMG" bs=512 count=$disk_sectors status=none
dd if="$BUILD_DIR/stage1.bin" of="$IMG" bs=512 count=1 conv=notrunc status=none
dd if="$BUILD_DIR/stage2.bin" of="$IMG" bs=512 seek=1 conv=notrunc status=none
dd if="$BUILD_DIR/kernel.bin" of="$IMG" bs=512 seek=$kernel_lba conv=notrunc status=none
dd if="$FS_IMG" of="$IMG" bs=512 seek=$ramdisk_lba conv=notrunc status=none

INPUT_FILE=""
if [[ ! -t 0 ]]; then
  INPUT_FILE="$(mktemp)"
  cat > "$INPUT_FILE"
fi

if [[ -z "${INPUT_FILE}" || ! -s "$INPUT_FILE" ]]; then
  set +e
  "$QEMU" -drive if=floppy,format=raw,file="$IMG" -drive if=ide,format=raw,file="$FS_IMG" $SNAPSHOT_FLAG -display none -serial stdio -monitor none -device isa-debug-exit,iobase=0xf4,iosize=0x04
  exit 0
fi

python3 - "$INPUT_FILE" "$QEMU" "$IMG" "$FS_IMG" "$SNAPSHOT_FLAG" <<'PY'
import os
import pty
import select
import subprocess
import sys
import time
import tty

input_path = sys.argv[1]
qemu = sys.argv[2]
img = sys.argv[3]
fs_img = sys.argv[4]
snapshot_flag = sys.argv[5]

args = [
    qemu,
    "-drive", f"if=floppy,format=raw,file={img}",
    "-drive", f"if=ide,format=raw,file={fs_img}",
    "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
]
if snapshot_flag:
    args.append(snapshot_flag)
args += [
    "-display", "none",
    "-serial", "stdio",
    "-monitor", "none",
]

with open(input_path, "rb") as f:
    data = f.read()

master_fd, slave_fd = pty.openpty()
tty.setraw(slave_fd)
proc = subprocess.Popen(args, stdin=slave_fd, stdout=slave_fd, stderr=None)
os.close(slave_fd)

time.sleep(0.2)
slow = os.environ.get("SLOPOS_SLOW_INPUT") == "1"
if slow:
    for b in data:
        os.write(master_fd, bytes([b]))
        time.sleep(0.01)
else:
    os.write(master_fd, data)

output = bytearray()
start = time.time()
while True:
    if proc.poll() is not None:
        ready, _, _ = select.select([master_fd], [], [], 0.1)
        if not ready:
            break
    ready, _, _ = select.select([master_fd], [], [], 0.1)
    if ready:
        try:
            chunk = os.read(master_fd, 4096)
        except OSError:
            break
        if not chunk:
            break
        output.extend(chunk)
    if time.time() - start > 15:
        proc.kill()
        break

sys.stdout.buffer.write(output)
os.close(master_fd)
PY

exit 0
