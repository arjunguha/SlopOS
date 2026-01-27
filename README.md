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
- `build/fs.img` (flat filesystem image with Scheme programs)
- `build/os.img` (1.44MB floppy image with both stages + kernel)

## Run

```bash
make run
```

Expected output:

```
Hello from C!
factorial(5) = 120
scheme factorial(5) = 120
1
2
1
2
1
2
1
2
1
2
```

QEMU powers off automatically after the output.

## Flat filesystem image

The packer builds a tiny flat filesystem (no directories) from `programs/`:

```bash
python3 scripts/mkfs.py programs build/fs.img
```

`init.scm` is loaded from this filesystem at boot.

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
