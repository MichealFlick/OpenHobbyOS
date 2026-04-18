#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/libxkbfile"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
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
    "$ROOT/user/lib/libxkbfile"
    --cross-file "$CROSS_FILE"
    --prefix /
    --bindir bin
    --libdir lib
    --includedir include
    --datadir share
    -Ddefault_library=static
    --buildtype release
)

if [[ -d "$BUILD_DIR/build" ]]; then
    "$MESON" "${meson_args[@]}" --reconfigure
else
    "$MESON" "${meson_args[@]}"
fi

"$MESON" compile -C "$BUILD_DIR/build"
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild
