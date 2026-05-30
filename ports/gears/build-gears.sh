#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/gears"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

CC="$TARGET-gcc"

CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie -std=c99"
CFLAGS="$CFLAGS -D_GNU_SOURCE -I$SYSROOT/include"

LDFLAGS="--sysroot=$SYSROOT"
LDFLAGS="$LDFLAGS -static -Wl,-T,$ROOT/user.ld -nostartfiles"
LDFLAGS="$LDFLAGS -Wl,--start-group -lc -lgcc -lm -ltinygl -Wl,--end-group"
LDFLAGS="$LDFLAGS -lopenhobbyosgloss"

echo "  CC    gears"
"$CC" -c $CFLAGS "$ROOT/ports/gears/gears.c" -o "$BUILD_DIR/gears.o"

echo "  LD    gears"
"$CC" \
    "$BUILD_DIR/gears.o" \
    $LDFLAGS \
    -o "$BUILD_DIR/gears"

echo "  INSTALL gears"
install -m 755 "$BUILD_DIR/gears" "$SYSROOT/bin/gears"

echo "gears built successfully"
