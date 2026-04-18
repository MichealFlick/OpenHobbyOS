#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/xtrans"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
SRC="$ROOT/user/lib/xtrans"
VERSION=$(sed -n "s/^AC_INIT(\\[[^]]*\\], \\[\\([^]]*\\)\\].*/\\1/p" "$SRC/configure.ac" | head -n1)

mkdir -p "$BUILD_DIR" "$SYSROOT/include/X11/Xtrans" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

while IFS= read -r file; do
    install -m 644 "$file" "$SYSROOT/include/X11/Xtrans/$(basename "$file")"
done < <(find "$SRC" -maxdepth 1 \( -name '*.h' -o -name '*.c' \) | sort)

cat > "$SYSROOT/lib/pkgconfig/xtrans.pc" <<EOF
prefix=/
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: XTrans
Description: Abstract transport helpers shared by X.Org
URL: https://gitlab.freedesktop.org/xorg/lib/libxtrans
Version: ${VERSION:-1.6.0}
Cflags: -I\${includedir} -D_DEFAULT_SOURCE -D_BSD_SOURCE -DHAS_FCHOWN -DHAS_STICKY_DIR_BIT
EOF
