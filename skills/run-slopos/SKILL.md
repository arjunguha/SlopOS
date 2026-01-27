---
name: run-slopos
description: Run the slopos BIOS/MBR bootloader in QEMU headless mode, rebuild the boot image when needed, and troubleshoot common run failures (missing packages, QEMU display errors, locked images, or stuck runs). Use when the user asks to run, boot, or verify slopos output.
---

# Run Slopos

## Overview

Run the slopos bootloader in QEMU headless mode and verify the text harness output.

## Workflow

1) Build the image
- Run `make` from the repo root.
- Ensure `build/os.img` is created.

2) Run headless and capture output
- Run `make run`.
- Expect the harness to print `SLOPOS BOOT`, `Text Harness`, and `All tests passed.`
- To quit headless QEMU, press `Ctrl-A` then `X`.

3) Troubleshoot
- If QEMU fails with GTK/display errors, ensure the run command uses `-nographic -serial stdio -monitor none`.
- If QEMU reports a write lock on `build/os.img`, find and stop the existing QEMU process.
- If output is missing, rebuild with `make clean && make` and re-run.

## Quick checks

- Confirm `nasm` and `qemu-system-x86` are installed on Ubuntu.
- Confirm the boot sector signature `0xAA55` exists in the last two bytes of `build/bootloader.bin`.
