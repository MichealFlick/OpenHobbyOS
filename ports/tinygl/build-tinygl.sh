#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/tinygl"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

TINYGL_SRC="$ROOT/user/lib/tinygl"
PATCH_FILE="$ROOT/ports/tinygl/tinygl-fixes.patch"

mkdir -p "$BUILD_DIR" "$SYSROOT/lib" "$SYSROOT/include/TGL"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

CC="$TARGET-gcc"
AR="$TARGET-ar"
RANLIB="$TARGET-ranlib"

# Apply comprehensive patch for buffer overflow fixes:
#   1. Pixel loop off-by-one (ztriangle.h: n>=3->4, n>=0->>0)
#   2. Textured path off-by-one (ztriangle.c: NB_INTERP-1->NB_INTERP, n>=0->>0)
#   3. nb_lines/x1/n bounds clamping (ztriangle.h: prevent OOB access)
#   4. zb->linesize alignment (zbuffer.c: use aligned xsize)
#   5. zpostprocess stride fix (use proper row pitch)
if grep -q 'while (n >= 4)' "$TINYGL_SRC/src/ztriangle.h" 2>/dev/null; then
    echo "  PATCH  tinygl-fixes (already applied)"
else
    echo "  PATCH  tinygl-fixes"
    patch -d "$TINYGL_SRC" -p1 < "$PATCH_FILE"
fi

CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie -std=c99"
CFLAGS="$CFLAGS -I$TINYGL_SRC/include -DNDEBUG"
CFLAGS="$CFLAGS -DNO_DEBUG_OUTPUT"

echo "Building libTinyGL.a..."
OBJS=""
for SRC in "$TINYGL_SRC/src/"*.c; do
    OBJ="$BUILD_DIR/$(basename "$SRC" .c).o"
    echo "  CC    $(basename "$SRC")"
    "$CC" -c $CFLAGS "$SRC" -o "$OBJ"
    OBJS="$OBJS $OBJ"
done

echo "  AR    libTinyGL.a"
"$AR" rcs "$BUILD_DIR/libTinyGL.a" $OBJS
"$RANLIB" "$BUILD_DIR/libTinyGL.a"

echo "  INSTALL libTinyGL.a"
install -m 644 "$BUILD_DIR/libTinyGL.a" "$SYSROOT/lib/libtinygl.a"
install -m 644 "$TINYGL_SRC/include/TGL/gl.h" "$SYSROOT/include/TGL/gl.h"
install -m 644 "$TINYGL_SRC/include/zbuffer.h" "$SYSROOT/include/zbuffer.h"
install -m 644 "$TINYGL_SRC/include/zfeatures.h" "$SYSROOT/include/zfeatures.h"

echo "tinygl built successfully"
