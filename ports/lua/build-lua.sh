#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/lua"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

LUA_SRC="$ROOT/user/lib/lua"

cleanup_patches() {
    git -C "$LUA_SRC" checkout -- . 2>/dev/null || true
}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin" "$SYSROOT/include" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

CC="$TARGET-gcc"
AR="$TARGET-ar"

CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie"
CFLAGS="$CFLAGS -I$LUA_SRC"

# Apply OHOS-specific patches before compilation
LUA_PATCHES="$ROOT/patches/lua"
if [ -d "$LUA_PATCHES" ]; then
    for patch in "$LUA_PATCHES"/*.patch; do
        [ -f "$patch" ] || continue
        echo "  PATCH $(basename "$patch")"
        git -C "$LUA_SRC" apply "$patch"
    done
fi

CORE_O="lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
       lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o \
       ltm.o lundump.o lvm.o lzio.o"
AUX_O="lauxlib.o"
LIB_O="lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o lstrlib.o \
       lutf8lib.o loadlib.o lcorolib.o linit.o"

ALL_O="$CORE_O $AUX_O $LIB_O"

compile() {
    local src="$1"
    local obj="$BUILD_DIR/$(basename "$src" .c).o"
    echo "  CC    $(basename "$src")"
    "$CC" -c $CFLAGS "$src" -o "$obj"
}

trap cleanup_patches EXIT

echo "Compiling Lua core library sources..."
for f in $CORE_O; do compile "$LUA_SRC/${f%.o}.c"; done

echo "Compiling Lua auxiliary sources..."
for f in $AUX_O; do compile "$LUA_SRC/${f%.o}.c"; done

echo "Compiling Lua library sources..."
for f in $LIB_O; do compile "$LUA_SRC/${f%.o}.c"; done

echo "  AR    liblua.a"
"$AR" rcs "$BUILD_DIR/liblua.a" \
    "$BUILD_DIR/lapi.o" "$BUILD_DIR/lcode.o" "$BUILD_DIR/lctype.o" \
    "$BUILD_DIR/ldebug.o" "$BUILD_DIR/ldo.o" "$BUILD_DIR/ldump.o" \
    "$BUILD_DIR/lfunc.o" "$BUILD_DIR/lgc.o" "$BUILD_DIR/llex.o" \
    "$BUILD_DIR/lmem.o" "$BUILD_DIR/lobject.o" "$BUILD_DIR/lopcodes.o" \
    "$BUILD_DIR/lparser.o" "$BUILD_DIR/lstate.o" "$BUILD_DIR/lstring.o" \
    "$BUILD_DIR/ltable.o" "$BUILD_DIR/ltm.o" "$BUILD_DIR/lundump.o" \
    "$BUILD_DIR/lvm.o" "$BUILD_DIR/lzio.o" \
    "$BUILD_DIR/lauxlib.o" \
    "$BUILD_DIR/lbaselib.o" "$BUILD_DIR/ldblib.o" "$BUILD_DIR/liolib.o" \
    "$BUILD_DIR/lmathlib.o" "$BUILD_DIR/loslib.o" "$BUILD_DIR/ltablib.o" \
    "$BUILD_DIR/lstrlib.o" "$BUILD_DIR/lutf8lib.o" "$BUILD_DIR/loadlib.o" \
    "$BUILD_DIR/lcorolib.o" "$BUILD_DIR/linit.o"
"$TARGET-ranlib" "$BUILD_DIR/liblua.a"

echo "Compiling lua.c..."
"$CC" -c $CFLAGS "$LUA_SRC/lua.c" -o "$BUILD_DIR/lua.o"

echo "  LD    lua"
"$CC" \
    --sysroot="$SYSROOT" \
    -static \
    -Wl,-T,"$SYSROOT/lib/user.ld" \
    -nostartfiles \
    -o "$BUILD_DIR/lua" \
    "$BUILD_DIR/lua.o" \
    "$BUILD_DIR/liblua.a" \
    -Wl,--start-group \
    -lc \
    -lgcc \
    -lm \
    -Wl,--end-group \
    -lopenhobbyosgloss \
    -Wl,--allow-multiple-definition

echo "  INSTALL liblua.a"
install -m 644 "$BUILD_DIR/liblua.a" "$SYSROOT/lib/liblua.a"

echo "  INSTALL lua headers"
for h in lua.h luaconf.h lualib.h lauxlib.h; do
    install -m 644 "$LUA_SRC/$h" "$SYSROOT/include/$h"
done

echo "  INSTALL lua binary"
install -m 755 "$BUILD_DIR/lua" "$SYSROOT/bin/lua"

cat > "$SYSROOT/lib/pkgconfig/lua.pc" <<EOF
prefix=/
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: Lua
Description: Lua scripting language
Version: 5.5.1
Requires:
Cflags: -I\${includedir}
Libs: -L\${libdir} -llua -lm
EOF

echo "lua built successfully"
