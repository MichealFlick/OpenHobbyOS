#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/zlib"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"
export CHOST="$TARGET"
export CC="${TARGET}-gcc"
export AR="${TARGET}-ar"
export RANLIB="${TARGET}-ranlib"
export CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie"
export LDFLAGS="--sysroot=$SYSROOT -static -nostartfiles -lopenhobbyosgloss"

if [[ ! -f "$BUILD_DIR/Makefile" ]]; then
    (
        cd "$BUILD_DIR"
        "$ROOT/user/lib/zlib/configure" \
            --static \
            --prefix=/ \
            --libdir=/lib \
            --includedir=/include
    )
fi

make -C "$BUILD_DIR" -j"$JOBS" libz.a
make -C "$BUILD_DIR" install prefix=/ libdir=/lib includedir=/include DESTDIR="$SYSROOT"
