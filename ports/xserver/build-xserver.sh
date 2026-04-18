#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/xserver"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
INSTALL_DIR=${3:-"$BUILD_DIR/install"}
TARGET=${TARGET:-i686-openhobbyos}
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"

mkdir -p "$BUILD_DIR" "$SYSROOT" "$INSTALL_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
INSTALL_DIR=$(cd "$INSTALL_DIR" && pwd)
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"
MESON=$("$ROOT/tools/ensure_meson.sh")

export PATH="$ROOT/toolchain/bin:$(dirname "$MESON"):$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

meson_args=(
    setup
    "$BUILD_DIR/build"
    "$ROOT/user/lib/xserver"
    --cross-file "$CROSS_FILE"
    --prefix /usr
    --bindir bin
    --libdir lib
    --includedir include
    --datadir share
    -Dfontrootdir=/usr/share/fonts/X11
    -Dxkb_dir=/usr/share/X11/xkb
    -Dxkb_output_dir=/tmp/server-xkb
    -Dxkb_bin_dir=/bin
    -Dsha1=libsha1
    -Dxorg=false
    -Dxephyr=false
    -Dxwayland=false
    -Dxnest=false
    -Dxvfb=true
    -Dxwin=false
    -Dxquartz=false
    -Dglamor=false
    -Dglx=false
    -Dxdmcp=false
    -Dxdm-auth-1=false
    -Dipv6=false
    -Dinput_thread=false
    -Dlisten_tcp=false
    -Dlisten_unix=true
    -Dlisten_local=true
    -Dpciaccess=false
    -Dudev=false
    -Dudev_kms=false
    -Dhal=false
    -Dsystemd_notify=false
    -Dsystemd_logind=false
    -Dvgahw=false
    -Ddpms=false
    -Dxf86bigfont=false
    -Dscreensaver=false
    -Dxres=true
    -Dxace=false
    -Dxselinux=false
    -Dxinerama=false
    -Dxcsecurity=false
    -Dxv=false
    -Dxvmc=false
    -Ddga=false
    -Dlinux_apm=false
    -Dlinux_acpi=false
    -Dmitshm=false
    -Dagp=false
    -Ddri1=false
    -Ddri2=false
    -Ddri3=false
    -Ddrm=false
    -Ddocs=false
    -Ddevel-docs=false
    -Ddocs-pdf=false
    --buildtype release
)

rm -rf "$BUILD_DIR/build"
"$MESON" "${meson_args[@]}"

"$MESON" compile -C "$BUILD_DIR/build" Xvfb
"$MESON" install -C "$BUILD_DIR/build" --destdir "$INSTALL_DIR" --no-rebuild
