#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/newlib"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-elf}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}
TARGET_CC=${TARGET_CC:-${TARGET}-gcc}
TARGET_AR=${TARGET_AR:-${TARGET}-ar}
TARGET_RANLIB=${TARGET_RANLIB:-${TARGET}-ranlib}
TARGET_INCLUDE_DIR="$SYSROOT/$TARGET/include"
TARGET_LIB_DIR="$SYSROOT/$TARGET/lib"

export PATH="$ROOT/toolchain/bin:$PATH"

mkdir -p "$BUILD_DIR" "$SYSROOT/lib" "$SYSROOT/include" "$SYSROOT/share"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
TARGET_INCLUDE_DIR="$SYSROOT/$TARGET/include"
TARGET_LIB_DIR="$SYSROOT/$TARGET/lib"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

if [[ ! -f "$BUILD_DIR/Makefile" || ! -f "$BUILD_DIR/$TARGET/newlib/Makefile" ]]; then
    (
        cd "$BUILD_DIR"
        "$ROOT/user/lib/newlib-cygwin/configure" \
            --target="$TARGET" \
            --prefix="$SYSROOT" \
            --disable-multilib \
            --disable-nls \
            --disable-werror \
            --enable-newlib-io-long-long \
            --enable-newlib-io-c99-formats \
            --enable-newlib-global-atexit \
            --enable-newlib-register-fini \
            --enable-newlib-reent-small \
            --disable-newlib-supplied-syscalls \
            --disable-libgloss \
            --with-newlib \
            --host="$(uname -m)-unknown-linux-gnu" \
            --build="$(uname -m)-unknown-linux-gnu" \
            --srcdir="$ROOT/user/lib/newlib-cygwin" \
            --cache-file="$BUILD_DIR/config.cache" \
            --disable-shared \
            --disable-threads \
            --disable-libssp \
            --disable-libstdcxx \
            --disable-libquadmath \
            --disable-libgomp \
            --disable-libatomic \
            --disable-libffi \
            --disable-libobjc \
            --disable-libada \
            --disable-libsanitizer \
            --disable-libvtv \
            --disable-libitm \
            --disable-libcc1 \
            --disable-gdb \
            --disable-sim \
            --disable-binutils \
            --disable-gas \
            --disable-ld \
            --disable-gprof \
            --disable-gprofng \
            --disable-gdbserver \
            --disable-gold \
            --disable-readline \
            --disable-dependency-tracking \
            --disable-maintainer-mode \
            --enable-languages=c \
            --enable-target-optspace \
            CFLAGS_FOR_TARGET="-O2 -ffreestanding -fno-pic -fno-pie"
    )
fi

make -C "$BUILD_DIR" -j"$JOBS" all-target-newlib
make -C "$BUILD_DIR" install-target-newlib

mkdir -p "$TARGET_INCLUDE_DIR"
while IFS= read -r header; do
    dest="$TARGET_INCLUDE_DIR/${header#"$ROOT/ports/newlib/openhobbyos/include/"}"
    mkdir -p "$(dirname "$dest")"
    install -m 644 "$header" "$dest"
done < <(find "$ROOT/ports/newlib/openhobbyos/include" -type f | sort)

compat_objects=()
pthread_object=""
while IFS= read -r source; do
    object="$BUILD_DIR/$(basename "${source%.*}").o"
    "$TARGET_CC" -c \
        -O2 -ffreestanding -fno-pic -fno-pie -fomit-frame-pointer \
        -D_GNU_SOURCE \
        -D_DEFAULT_SOURCE \
        -D_POSIX_C_SOURCE=200809L \
        -D_POSIX_TIMERS=200809L \
        -D_POSIX_MONOTONIC_CLOCK=200809L \
        -D_POSIX_CLOCK_SELECTION=200809L \
        -I"$ROOT/ports/newlib/openhobbyos/include" \
        -isystem "$TARGET_INCLUDE_DIR" \
        -idirafter "$ROOT/include" \
        "$source" \
        -o "$object"
    compat_objects+=("$object")
    if [[ "$(basename "$source")" == "compat_pthread.c" ]]; then
        pthread_object="$object"
    fi
done < <(find "$ROOT/ports/newlib/openhobbyos" -maxdepth 1 \( -name '*.c' -o -name '*.S' \) | sort)

"$TARGET_AR" rcs "$BUILD_DIR/libopenhobbyosgloss.a" "${compat_objects[@]}"
"$TARGET_RANLIB" "$BUILD_DIR/libopenhobbyosgloss.a"

if [[ -n "$pthread_object" ]]; then
    "$TARGET_AR" rcs "$BUILD_DIR/libpthread.a" "$pthread_object"
    "$TARGET_RANLIB" "$BUILD_DIR/libpthread.a"
fi

mkdir -p "$TARGET_LIB_DIR"

install -m 644 "$BUILD_DIR/crt0.o" "$SYSROOT/lib/crt0.o"
install -m 644 "$BUILD_DIR/libopenhobbyosgloss.a" "$SYSROOT/lib/libopenhobbyosgloss.a"
install -m 644 "$ROOT/ports/newlib/openhobbyos/openhobbyos.specs" "$SYSROOT/lib/openhobbyos.specs"
install -m 644 "$BUILD_DIR/libopenhobbyosgloss.a" "$TARGET_LIB_DIR/libdl.a"
if [[ -f "$BUILD_DIR/libpthread.a" ]]; then
    install -m 644 "$BUILD_DIR/libpthread.a" "$TARGET_LIB_DIR/libpthread.a"
fi

runtime_archive=""
if runtime_archive=$(clang -m32 -rtlib=compiler-rt --print-file-name=libclang_rt.builtins-i386.a 2>/dev/null); then
    if [[ ! -f "$runtime_archive" ]]; then
        runtime_archive=""
    fi
fi

if [[ -z "$runtime_archive" ]]; then
    runtime_archive=$(gcc -m32 -print-libgcc-file-name 2>/dev/null || true)
    if [[ "$runtime_archive" == "libgcc.a" || ! -f "$runtime_archive" ]]; then
        runtime_archive=""
    fi
fi

if [[ -n "$runtime_archive" ]]; then
    # GCC emits helper calls for 32-bit math even in plain C. We ship a real
    # builtin runtime so larger userland binaries link without leaning on host
    # multilib packages.
    install -m 644 "$runtime_archive" "$TARGET_LIB_DIR/libgcc.a"
    install -m 644 "$runtime_archive" "$TARGET_LIB_DIR/libgcc_eh.a"
fi
