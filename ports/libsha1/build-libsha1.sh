#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/libsha1"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

mkdir -p "$BUILD_DIR" "$SYSROOT/include" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

"$TARGET-gcc" -c \
    --sysroot="$SYSROOT" \
    -O2 -ffreestanding -fno-pic -fno-pie \
    -I"$ROOT/ports/libsha1" \
    "$ROOT/ports/libsha1/libsha1.c" \
    -o "$BUILD_DIR/libsha1.o"

"$TARGET-ar" rcs "$BUILD_DIR/libsha1.a" "$BUILD_DIR/libsha1.o"
"$TARGET-ranlib" "$BUILD_DIR/libsha1.a"

install -m 644 "$ROOT/ports/libsha1/libsha1.h" "$SYSROOT/include/libsha1.h"
install -m 644 "$BUILD_DIR/libsha1.a" "$SYSROOT/lib/libsha1.a"

cat > "$SYSROOT/lib/pkgconfig/libsha1.pc" <<'EOF'
prefix=/
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: libsha1
Description: Small standalone SHA1 implementation for OpenHobbyOS ports
Version: 1.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lsha1
EOF
