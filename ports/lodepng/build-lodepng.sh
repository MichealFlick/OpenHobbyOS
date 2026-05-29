#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/lodepng"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

mkdir -p "$BUILD_DIR" "$SYSROOT/include" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

"$TARGET-gcc" -x c -c \
    --sysroot="$SYSROOT" \
    -O2 -ffreestanding -fno-pic -fno-pie \
    -DLODEPNG_NO_COMPILE_CPP \
    -DLODEPNG_NO_COMPILE_DISK \
    "$ROOT/user/lib/lodepng/lodepng.cpp" \
    -o "$BUILD_DIR/lodepng.o"

"$TARGET-ar" rcs "$BUILD_DIR/liblodepng.a" "$BUILD_DIR/lodepng.o"
"$TARGET-ranlib" "$BUILD_DIR/liblodepng.a"

install -m 644 "$ROOT/user/lib/lodepng/lodepng.h" "$SYSROOT/include/lodepng.h"
install -m 644 "$BUILD_DIR/liblodepng.a" "$SYSROOT/lib/liblodepng.a"

cat > "$SYSROOT/lib/pkgconfig/lodepng.pc" <<'EOF'
prefix=/
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: lodepng
Description: PNG encoder and decoder in C and C++
Version: 20260119
Cflags: -I${includedir}
Libs: -L${libdir} -llodepng
EOF
