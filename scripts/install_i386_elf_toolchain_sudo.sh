#!/usr/bin/env bash
set -euo pipefail

# Build and install a freestanding i386-elf cross toolchain system-wide.
# Uses /usr/local by default. Requires sudo.

PREFIX="${PREFIX:-/usr/local}"
TARGET="${TARGET:-i386-elf}"
BINUTILS_VER="${BINUTILS_VER:-2.42}"
GCC_VER="${GCC_VER:-13.2.0}"
JOBS="${JOBS:-$(nproc)}"
BUILD_ROOT="${BUILD_ROOT:-/tmp/i386-elf-toolchain}"

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (e.g., sudo $0)"
  exit 1
fi

apt-get update
apt-get install -y build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo curl

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT"
cd "$BUILD_ROOT"

curl -LO "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz"
tar xf "binutils-$BINUTILS_VER.tar.xz"
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

cd "$BUILD_ROOT"
curl -LO "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz"
tar xf "gcc-$GCC_VER.tar.xz"
cd "gcc-$GCC_VER"
./contrib/download_prerequisites
cd "$BUILD_ROOT"
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
echo "Done. Verify with:"
echo "  i386-elf-gcc --version"
