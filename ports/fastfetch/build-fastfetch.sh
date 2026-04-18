#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/fastfetch"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
INSTALL_DIR=${3:-"$BUILD_DIR/install"}

mkdir -p "$BUILD_DIR" "$SYSROOT" "$INSTALL_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
INSTALL_DIR=$(cd "$INSTALL_DIR" && pwd)

rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

FF_CFLAGS="-O2 -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_POSIX_TIMERS=200809L -D_POSIX_MONOTONIC_CLOCK=200809L -D_POSIX_CLOCK_SELECTION=200809L -Dgetline=__getline -Dgetdelim=__getdelim"
FF_RELEASE_CFLAGS="-O3 -DNDEBUG -Wno-char-subscripts"

cmake -S "$ROOT/user/lib/fastfetch" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/fastfetch/openhobbyos-toolchain.cmake" \
    -DOPENHOBBYOS_SYSROOT="$SYSROOT" \
    -DCMAKE_INSTALL_PREFIX=/ \
    -DCMAKE_C_FLAGS="$FF_CFLAGS" \
    -DCMAKE_C_FLAGS_RELEASE="$FF_RELEASE_CFLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="--sysroot=$SYSROOT -static -Wl,-T,$ROOT/user.ld -nostartfiles -lopenhobbyosgloss" \
    -DCMAKE_DL_LIBS= \
    -DCMAKE_BUILD_TYPE=Release \
    -DBINARY_LINK_TYPE=static \
    -DBUILD_FLASHFETCH=OFF \
    -DBUILD_TESTS=OFF \
    -DENABLE_LIBZFS=OFF \
    -DENABLE_WORDEXP=OFF \
    -DENABLE_THREADS=OFF \
    -DENABLE_VULKAN=OFF \
    -DENABLE_WAYLAND=OFF \
    -DENABLE_XCB_RANDR=OFF \
    -DENABLE_XRANDR=OFF \
    -DENABLE_DRM=OFF \
    -DENABLE_DRM_AMDGPU=OFF \
    -DENABLE_GIO=OFF \
    -DENABLE_DCONF=OFF \
    -DENABLE_DBUS=OFF \
    -DENABLE_SQLITE3=OFF \
    -DENABLE_RPM=OFF \
    -DENABLE_IMAGEMAGICK7=OFF \
    -DENABLE_IMAGEMAGICK6=OFF \
    -DENABLE_CHAFA=OFF \
    -DENABLE_EGL=OFF \
    -DENABLE_GLX=OFF \
    -DENABLE_OPENCL=OFF \
    -DENABLE_PULSE=OFF \
    -DENABLE_DDCUTIL=OFF \
    -DENABLE_ELF=OFF \
    -DENABLE_ZLIB=OFF

cmake --build "$BUILD_DIR" --target fastfetch
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
