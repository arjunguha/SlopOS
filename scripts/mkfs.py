#!/usr/bin/env python3
import os
import struct
import sys

MAGIC = b"SLOPFS1\0"
VERSION = 1
ENTRY_NAME_LEN = 64
ENTRY_SIZE = 64 + 4 + 4 + 4
SUPERBLOCK_SIZE = 512
BOOT_HEADER_SIZE = 8


def align(value, multiple):
    return (value + multiple - 1) // multiple * multiple


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <input-dir> <output-img>", file=sys.stderr)
        return 1

    in_dir = sys.argv[1]
    out_path = sys.argv[2]

    if not os.path.isdir(in_dir):
        print(f"error: input dir not found: {in_dir}", file=sys.stderr)
        return 1

    files = []
    boot_path = os.path.join(in_dir, "boot.scm")
    if not os.path.isfile(boot_path):
        print("error: boot.scm not found in input dir", file=sys.stderr)
        return 1

    with open(boot_path, "rb") as f:
        boot_data = f.read()
    boot_len = len(boot_data)

    for name in sorted(os.listdir(in_dir)):
        path = os.path.join(in_dir, name)
        if os.path.isfile(path):
            if name == "boot.scm":
                continue
            if len(name.encode("ascii", errors="strict")) > ENTRY_NAME_LEN:
                print(f"error: filename too long: {name}", file=sys.stderr)
                return 1
            files.append((name, path))

    fs_offset = align(BOOT_HEADER_SIZE + boot_len, 512)
    dir_offset = fs_offset + SUPERBLOCK_SIZE
    dir_length = len(files) * ENTRY_SIZE
    data_offset = align(dir_offset + dir_length, 512)

    file_entries = []
    data_cursor = data_offset
    for name, path in files:
        with open(path, "rb") as f:
            data = f.read()
        file_entries.append((name, data_cursor, len(data), data))
        data_cursor += len(data)

    total_size = align(data_cursor, 512)
    img = bytearray(total_size)

    struct.pack_into("<II", img, 0, boot_len, fs_offset)
    img[BOOT_HEADER_SIZE:BOOT_HEADER_SIZE + boot_len] = boot_data
    struct.pack_into("<8sIIII", img, fs_offset, MAGIC, VERSION, dir_offset - fs_offset, dir_length, data_offset - fs_offset)

    dir_pos = dir_offset
    for name, data_off, data_len, _ in file_entries:
        name_bytes = name.encode("ascii")
        name_field = name_bytes + b"\0" * (ENTRY_NAME_LEN - len(name_bytes))
        img[dir_pos:dir_pos + ENTRY_NAME_LEN] = name_field
        struct.pack_into("<III", img, dir_pos + ENTRY_NAME_LEN, data_off, data_len, 0)
        dir_pos += ENTRY_SIZE

    for _, data_off, _, data in file_entries:
        img[data_off:data_off + len(data)] = data

    with open(out_path, "wb") as f:
        f.write(img)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
