#!/usr/bin/env bash
set -euo pipefail

# Build a freestanding i386-elf cross toolchain in user space.
# This avoids sudo and installs into $PREFIX (default: $HOME/opt/cross).

PREFIX="${PREFIX:-$HOME/opt/cross}"
TARGET="${TARGET:-i386-elf}"
BINUTILS_VER="${BINUTILS_VER:-2.42}"
GCC_VER="${GCC_VER:-13.2.0}"
JOBS="${JOBS:-$(nproc)}"

export PATH="$PREFIX/bin:$PATH"

mkdir -p "$HOME/src"
cd "$HOME/src"

if [ ! -d "binutils-$BINUTILS_VER" ]; then
  curl -LO "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz"
  tar xf "binutils-$BINUTILS_VER.tar.xz"
fi

mkdir -p build-binutils
cd build-binutils
"../binutils-$BINUTILS_VER/configure" \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --with-sysroot \
  --disable-nls \
  --disable-werror
make -j"$JOBS"
make install

cd "$HOME/src"
if [ ! -d "gcc-$GCC_VER" ]; then
  curl -LO "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz"
  tar xf "gcc-$GCC_VER.tar.xz"
  cd "gcc-$GCC_VER"
  ./contrib/download_prerequisites
  cd "$HOME/src"
fi

mkdir -p build-gcc
cd build-gcc
"../gcc-$GCC_VER/configure" \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --disable-nls \
  --enable-languages=c \
  --without-headers
make -j"$JOBS" all-gcc
make -j"$JOBS" all-target-libgcc
make install-gcc
make install-target-libgcc

echo ""
echo "Done. Add this to your shell rc (bashrc/zshrc):"
echo "  export PATH=\"$PREFIX/bin:\$PATH\""
echo "Then verify: i386-elf-gcc --version"
