#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/x11"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
SRC="$ROOT/user/lib/libx11"
PROTO_SRC="$ROOT/user/lib/xorgproto"
VERSION=$(sed -n "s/^AC_INIT(\\[[^]]*\\], \\[\\([^]]*\\)\\].*/\\1/p" "$SRC/configure.ac" | head -n1)

mkdir -p "$BUILD_DIR/generated" "$SYSROOT/include/X11" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

python3 - "$PROTO_SRC/include/X11/Xatom.h" "$BUILD_DIR/generated/generated_atoms.h" <<'PY'
import re
import sys
from pathlib import Path

src = Path(sys.argv[1]).read_text()
out = Path(sys.argv[2])
atoms = []
for name, value in re.findall(r'^#define XA_([A-Za-z0-9_]+)\s+\(\(Atom\)\s*([0-9]+)\)', src, re.M):
    atoms.append((int(value), name))
atoms.sort()
with out.open('w') as f:
    f.write("static const builtin_atom_t kBuiltinAtoms[] = {\n")
    for value, name in atoms:
        f.write(f"    {{ (Atom){value}, \"{name}\" }},\n")
    f.write("};\n")
PY

python3 - "$PROTO_SRC/include/X11/keysymdef.h" "$BUILD_DIR/generated/generated_keysyms.h" <<'PY'
import re
import sys
from pathlib import Path

src = Path(sys.argv[1]).read_text()
out = Path(sys.argv[2])
seen = {}
for name, value in re.findall(r'^#define XK_([A-Za-z0-9_]+)\s+(0x[0-9A-Fa-f]+)', src, re.M):
    key = int(value, 16)
    seen.setdefault(key, name)
with out.open('w') as f:
    f.write("static const builtin_keysym_t kBuiltinKeysyms[] = {\n")
    for key, name in sorted(seen.items()):
        f.write(f"    {{ (KeySym)0x{key:08x}UL, \"{name}\" }},\n")
    f.write("};\n")
PY

while IFS= read -r header; do
    rel=${header#"$SRC/include/"}
    dest="$SYSROOT/include/$rel"
    mkdir -p "$(dirname "$dest")"
    if [[ ! -f "$dest" ]]; then
        install -m 644 "$header" "$dest"
    fi
done < <(find "$SRC/include" -type f -name '*.h' | sort)

"${TARGET}-gcc" -c \
    --sysroot="$SYSROOT" \
    -O2 -ffreestanding -fno-pic -fno-pie \
    -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
    -I"$SYSROOT/include" \
    -I"$BUILD_DIR/generated" \
    -I"$SRC/include" \
    "$ROOT/ports/x11/compat_xlib.c" \
    -o "$BUILD_DIR/compat_xlib.o"

"${TARGET}-ar" rcs "$BUILD_DIR/libX11.a" "$BUILD_DIR/compat_xlib.o"
"${TARGET}-ranlib" "$BUILD_DIR/libX11.a"
install -m 644 "$BUILD_DIR/libX11.a" "$SYSROOT/lib/libX11.a"

cat > "$SYSROOT/lib/pkgconfig/x11.pc" <<EOF
prefix=/
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: X11
Description: OpenHobbyOS Xlib compatibility layer backed by upstream headers
URL: https://gitlab.freedesktop.org/xorg/lib/libX11/
Version: ${VERSION:-1.8.12}
Requires: xproto kbproto
Cflags: -I\${includedir}
Libs: -L\${libdir} -lX11
EOF
