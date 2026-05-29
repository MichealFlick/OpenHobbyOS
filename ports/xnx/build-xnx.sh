#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/xnx"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
INSTALL_DIR=${3:-"$BUILD_DIR/install"}

TARGET=${TARGET:-i686-openhobbyos}
export PATH="$ROOT/toolchain/bin:$PATH"

CC=${CC:-"$TARGET-gcc"}
AR=${AR:-"$TARGET-ar"}
RANLIB=${RANLIB:-"$TARGET-ranlib"}
PKG_CONFIG=${PKG_CONFIG:-"$TARGET-pkg-config"}

mkdir -p "$BUILD_DIR" "$SYSROOT" "$INSTALL_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
INSTALL_DIR=$(cd "$INSTALL_DIR" && pwd)

rm -rf "$BUILD_DIR/obj" "$BUILD_DIR/bin" "$BUILD_DIR/lib"
mkdir -p \
    "$BUILD_DIR/obj" \
    "$BUILD_DIR/bin" \
    "$BUILD_DIR/lib" \
    "$INSTALL_DIR/bin" \
    "$INSTALL_DIR/lib" \
    "$INSTALL_DIR/include/xnx" \
    "$SYSROOT/bin" \
    "$SYSROOT/include/xnx"

PIXMAN_CFLAGS=($("$PKG_CONFIG" --cflags pixman-1))
PIXMAN_LIBS=($("$PKG_CONFIG" --libs pixman-1))
LODEPNG_CFLAGS=($("$PKG_CONFIG" --cflags lodepng))

COMMON_CFLAGS=(
    --sysroot="$SYSROOT"
    -O2
    -fno-jump-tables
    -ffreestanding
    -fno-pic
    -fno-pie
    -D_GNU_SOURCE
    -D_DEFAULT_SOURCE
    -D_XOPEN_SOURCE=700
    -D_POSIX_C_SOURCE=200809L
    -Wno-char-subscripts
    -I"$ROOT/user/xnx"
    -I"$ROOT/user/lib/xnx"
)

compile_object() {
    local src=$1
    local out=$2
    "$CC" "${COMMON_CFLAGS[@]}" "${PIXMAN_CFLAGS[@]}" "${LODEPNG_CFLAGS[@]}" -c "$src" -o "$out"
}

build_compositor() {
    compile_object "$ROOT/user/xnx/compositor.c" "$BUILD_DIR/obj/compositor.o"
    compile_object "$ROOT/user/xnx/strtod_stub.c" "$BUILD_DIR/obj/strtod_stub.o"
    "$CC" \
        --sysroot="$SYSROOT" \
        -static \
        -Wl,-T,"$SYSROOT/lib/user.ld" \
        -nostartfiles \
        -o "$BUILD_DIR/bin/xnx-compositor" \
        "$BUILD_DIR/obj/compositor.o" \
        "$BUILD_DIR/obj/strtod_stub.o" \
        -Wl,--start-group \
        "${PIXMAN_LIBS[@]}" \
        -llodepng \
        -lc \
        -lgcc \
        -Wl,--end-group \
        -lopenhobbyosgloss

    install -m 755 "$BUILD_DIR/bin/xnx-compositor" "$INSTALL_DIR/bin/xnx-compositor"
    install -m 755 "$BUILD_DIR/bin/xnx-compositor" "$SYSROOT/bin/xnx-compositor"
}

build_client_lib() {
    "$CC" "${COMMON_CFLAGS[@]}" -c "$ROOT/user/lib/xnx/xnx.c" -o "$BUILD_DIR/obj/xnx.o"
    "$AR" rcs "$BUILD_DIR/lib/libxnx.a" "$BUILD_DIR/obj/xnx.o"
    "$RANLIB" "$BUILD_DIR/lib/libxnx.a"

    install -m 644 "$BUILD_DIR/lib/libxnx.a" "$INSTALL_DIR/lib/libxnx.a"
    install -m 644 "$BUILD_DIR/lib/libxnx.a" "$SYSROOT/lib/libxnx.a"
    install -m 644 "$ROOT/user/lib/xnx/xnx.h" "$INSTALL_DIR/include/xnx/xnx.h"
    install -m 644 "$ROOT/user/lib/xnx/xnx.h" "$SYSROOT/include/xnx/xnx.h"
    install -m 644 "$ROOT/user/xnx/protocol.h" "$INSTALL_DIR/include/xnx/protocol.h"
    install -m 644 "$ROOT/user/xnx/protocol.h" "$SYSROOT/include/xnx/protocol.h"
}

build_demo() {
    compile_object "$ROOT/user/xnx/demo.c" "$BUILD_DIR/obj/demo.o"
    "$CC" \
        --sysroot="$SYSROOT" \
        -static \
        -Wl,-T,"$SYSROOT/lib/user.ld" \
        -nostartfiles \
        -o "$BUILD_DIR/bin/xnx-demo" \
        "$BUILD_DIR/obj/demo.o" \
        "$BUILD_DIR/lib/libxnx.a" \
        -Wl,--start-group \
        -lc \
        -lgcc \
        -Wl,--end-group \
        -lopenhobbyosgloss

    install -m 755 "$BUILD_DIR/bin/xnx-demo" "$INSTALL_DIR/bin/xnx-demo"
    install -m 755 "$BUILD_DIR/bin/xnx-demo" "$SYSROOT/bin/xnx-demo"
}

build_compositor
build_client_lib
build_demo

echo "XNX built successfully"
