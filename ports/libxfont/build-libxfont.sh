#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/libxfont"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
SRC="$ROOT/user/lib/libxfont"
VERSION=$(sed -n "s/^AC_INIT(\\[[^]]*\\], \\[\\([^]]*\\)\\].*/\\1/p" "$SRC/configure.ac" | head -n1)

mkdir -p "$BUILD_DIR/obj" "$SYSROOT/include/X11/fonts" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

cat > "$BUILD_DIR/config.h" <<'EOF'
#ifndef OPENHOBBYOS_LIBXFONT_CONFIG_H
#define OPENHOBBYOS_LIBXFONT_CONFIG_H

#define _GNU_SOURCE 1
#define HAVE_STDINT_H 1
#define HAVE_FLOAT_H 1
#define HAVE_READLINK 1

#define XFONT_FONTFILE 1
#define XFONT_BITMAP 1
#define XFONT_BDFFORMAT 1
#define XFONT_PCFFORMAT 1
#define XFONT_BUILTINS 1
#define X_GZIP_FONT_COMPRESSION 1

#endif
EOF

sources=(
    src/stubs/atom.c
    src/stubs/libxfontstubs.c
    src/util/fontaccel.c
    src/util/fontnames.c
    src/util/fontutil.c
    src/util/fontxlfd.c
    src/util/format.c
    src/util/miscutil.c
    src/util/patcache.c
    src/util/private.c
    src/util/utilbitmap.c
    src/util/reallocarray.c
    src/util/realpath.c
    src/util/strlcat.c
    src/util/strlcpy.c
    src/fontfile/bitsource.c
    src/fontfile/bufio.c
    src/fontfile/decompress.c
    src/fontfile/defaults.c
    src/fontfile/dirfile.c
    src/fontfile/fileio.c
    src/fontfile/filewr.c
    src/fontfile/fontdir.c
    src/fontfile/fontencc.c
    src/fontfile/fontfile.c
    src/fontfile/fontscale.c
    src/fontfile/gunzip.c
    src/fontfile/register.c
    src/fontfile/renderers.c
    src/fontfile/catalogue.c
    src/bitmap/bitmap.c
    src/bitmap/bitmapfunc.c
    src/bitmap/bitmaputil.c
    src/bitmap/bitscale.c
    src/bitmap/fontink.c
    src/bitmap/bdfread.c
    src/bitmap/bdfutils.c
    src/bitmap/pcfread.c
    src/bitmap/pcfwrite.c
    src/builtins/dir.c
    src/builtins/file.c
    src/builtins/fonts.c
    src/builtins/fpe.c
    src/builtins/render.c
)

objects=()
for source in "${sources[@]}"; do
    base=${source#src/}
    obj="$BUILD_DIR/obj/${base%.c}.o"
    mkdir -p "$(dirname "$obj")"
    "${TARGET}-gcc" -c \
        --sysroot="$SYSROOT" \
        -O2 -ffreestanding -fno-pic -fno-pie \
        -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
        -DHAVE_CONFIG_H \
        -I"$BUILD_DIR" \
        -I"$SRC" \
        -I"$SRC/include" \
        -I"$SYSROOT/include" \
        "$SRC/$source" \
        -o "$obj"
    objects+=("$obj")
done

"${TARGET}-ar" rcs "$BUILD_DIR/libXfont2.a" "${objects[@]}"
"${TARGET}-ranlib" "$BUILD_DIR/libXfont2.a"
install -m 644 "$BUILD_DIR/libXfont2.a" "$SYSROOT/lib/libXfont2.a"
install -m 644 "$SRC/include/X11/fonts/libxfont2.h" "$SYSROOT/include/X11/fonts/libxfont2.h"

cat > "$SYSROOT/lib/pkgconfig/xfont2.pc" <<EOF
prefix=/
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: Xfont2
Description: OpenHobbyOS bitmap-focused libXfont2 build from upstream sources
Version: ${VERSION:-2.0.7}
Requires: xproto fontsproto
Requires.private: fontenc zlib
Cflags: -I\${includedir}
Libs: -L\${libdir} -lXfont2 -lz
Libs.private: -lz -lm
EOF
