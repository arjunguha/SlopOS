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
```

QEMU powers off automatically after the output.

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
