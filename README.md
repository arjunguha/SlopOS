# slopos

Tiny BIOS/MBR bootloader with a two-stage loader. Stage 2 loads a freestanding
32-bit C kernel, probes the E820 memory map, switches to protected mode, and
prints a simple test harness over a basic console (serial + VGA). The kernel
prints from C, runs a tiny R5RS-ish Scheme interpreter, and then powers off.

## Packages (Ubuntu)

You said you already have `build-essential`. Install the rest with:

```bash
sudo apt-get update
sudo apt-get install -y nasm binutils make qemu-system-x86
```

## Cross compiler

Install a freestanding `i386-elf-gcc` toolchain (no sudo required):

```bash
./scripts/install_i386_elf_toolchain.sh
```

Then add it to your PATH:

```bash
export PATH="$HOME/opt/cross/bin:$PATH"
```

## Build

```bash
make
```

This produces:

- `build/stage1.bin` (512-byte boot sector)
- `build/stage2.bin` (second-stage loader)
- `build/kernel.bin` (freestanding 32-bit kernel image)
- `build/fs.img` (boot script + flat filesystem image with Scheme programs)
- `build/os.img` (1.44MB floppy image with both stages + kernel)

## Run

```bash
make run
```

To run a specific init program by name (e.g., `init_scripts/alt.scm`):

```bash
make run-init NAME=alt
```

To build images for every init script:

```bash
make images
```

Expected output:

```
Hello from C!
factorial(5) = 120
120
```

QEMU powers off automatically after the output.

## Flat filesystem image

The packer builds a boot + filesystem image from `programs/`:

```bash
python3 scripts/mkfs.py programs build/fs.img
```

`boot.scm` is read from a fixed location at the start of the image. It defines
filesystem helpers and then loads `init.scm` from the flat filesystem region.

## Init scripts

Init programs live in `init_scripts/`. The default image uses
`init_scripts/default.scm`.

### Boot + filesystem image layout

The `build/fs.img` format is:

- Offset 0x0000: `boot_len` (uint32 LE)
- Offset 0x0004: `fs_offset` (uint32 LE)
- Offset 0x0008: `boot.scm` bytes (length = `boot_len`)
- Offset `fs_offset`: superblock

Superblock (512 bytes at `fs_offset`):
- Magic: 8 bytes `SLOPFS1\0`
- Version: uint32 LE
- Dir offset (relative to `fs_offset`): uint32 LE
- Dir length (bytes): uint32 LE
- Data offset (relative to `fs_offset`): uint32 LE

Directory entries (packed, 76 bytes each):
- Name: 64 bytes ASCII, null-padded
- File offset (relative to `fs_offset`): uint32 LE
- File length (bytes): uint32 LE
- Reserved: uint32 LE (currently 0)

Notes:
- Filename length is limited to 64 ASCII bytes (longer names are rejected by the packer).
- `fs_offset` is aligned to 512 bytes; directory and data offsets are relative to `fs_offset`.

## Scheme interpreter (Linux)

Build and run the host test interpreter:

```bash
make scheme-host
./build/scheme-host
```

You can pass a Scheme file to run:

```bash
./build/scheme-host path/to/program.scm
```

To test disk-backed loading in the host interpreter, pass the filesystem image
as a second argument:

```bash
./build/scheme-host path/to/program.scm build/fs.img
```
