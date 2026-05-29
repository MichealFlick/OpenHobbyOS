#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/ffmpeg"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

FFMPEG_SRC="$ROOT/user/lib/ffmpeg"

mkdir -p "$BUILD_DIR" "$SYSROOT/include" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

NEWLIB_SYSROOT="$SYSROOT/i686-elf"

if [ ! -f "$NEWLIB_SYSROOT/lib/libc.a" ]; then
    echo "ERROR: newlib must be built first (run 'make ports-newlib')"
    echo "  (looked for $NEWLIB_SYSROOT/lib/libc.a)"
    exit 1
fi

# The toolchain wrappers handle sysroot and -isystem automatically.
# We use i686-openhobbyos- wrappers for cc/ld and fall back to
# i686-elf- for binutils that don't have openhobbyos wrappers.
PORT_DIR="$ROOT/ports/ffmpeg"

CC="$TARGET-gcc"
CXX="$TARGET-g++"
AR="i686-elf-ar"
RANLIB="i686-elf-ranlib"
STRIP="i686-elf-strip"
PKG_CONFIG="i686-elf-pkg-config"

CFLAGS="-O2 -ffreestanding -fno-pic -fno-pie -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

# Pre-compile a minimal startup stub needed for the configure linker test.
STUB_OBJ="$BUILD_DIR/ffmpeg-stub.o"
"$CC" $CFLAGS -c "$PORT_DIR/stub.c" -o "$STUB_OBJ"

EXTRA_CFLAGS="$CFLAGS"
EXTRA_LDFLAGS="-nostartfiles -Wl,-T,$SYSROOT/lib/user.ld -static $STUB_OBJ -Wl,--start-group -lopenhobbyosgloss -lc -lgcc -Wl,--end-group -Wl,--allow-multiple-definition"

cd "$FFMPEG_SRC"

echo "Configuring FFmpeg for $TARGET..."
./configure \
    --prefix="$SYSROOT" \
    --cross-prefix="${TARGET}-" \
    --target-os=linux \
    --arch=x86_32 \
    --cpu=i686 \
    --enable-cross-compile \
    --disable-programs \
    --disable-doc \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-network \
    --disable-pthreads \
    --disable-w32threads \
    --disable-os2threads \
    --disable-asm \
    --disable-inline-asm \
    --disable-x86asm \
    --disable-mmx \
    --disable-mmxext \
    --disable-sse \
    --disable-sse2 \
    --disable-sse3 \
    --disable-ssse3 \
    --disable-sse4 \
    --disable-sse42 \
    --disable-avx \
    --disable-avx2 \
    --disable-fma3 \
    --disable-fma4 \
    --disable-avx512 \
    --disable-debug \
    --disable-optimizations \
    --enable-static \
    --disable-shared \
    --enable-small \
    --enable-pic \
    --cc="$CC" \
    --cxx="$CXX" \
    --ar="$AR" \
    --ranlib="$RANLIB" \
    --strip="$STRIP" \
    --extra-cflags="$EXTRA_CFLAGS" \
    --extra-ldflags="$EXTRA_LDFLAGS" \
    --enable-avutil \
    --enable-avcodec \
    --enable-avformat \
    --enable-swresample \
    --enable-swscale \
    --enable-avdevice \
    --enable-avfilter \
    --disable-encoders \
    --disable-hwaccels \
    --disable-indevs \
    --disable-outdevs \
    --disable-devices \
    --disable-filters \
    --disable-bsfs \
    --enable-decoder=mpeg4,h264,aac,mp3,vp8,vp9,theora,flac,vorbis,wmav1,wmav2,pcm_s16le,pcm_u8,png \
    --enable-demuxer=avi,mov,flv,mkv,mp4,wav,ogg,asf,image2 \
    --enable-muxer=rawvideo,avi,wav,mp4,image2 \
    --enable-protocol=file \
    --enable-parser=h264,aac,mpeg4video,vp8,vp9,theora,vorbis \
    --enable-bsf=h264_mp4toannexb,aac_adtstoasc

echo ""
echo "Building FFmpeg..."
make -j$(nproc) clean 2>/dev/null || true
make -j$(nproc)

echo ""
echo "Installing to sysroot..."
make install

echo ""
echo "Generating pkg-config files..."

for lib in libavutil libavcodec libavformat libswresample libswscale libavdevice libavfilter; do
    lib_name="${lib#lib}"
    lib_name="${lib_name%.a}"
    
    if [ -f "$SYSROOT/lib/$lib.a" ]; then
        cat > "$SYSROOT/lib/pkgconfig/${lib}.pc" <<EOF
prefix=/
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: $lib
Description: FFmpeg $lib
Version: $(grep 'VERSION' "$FFMPEG_SRC/RELEASE" 2>/dev/null || echo "7.0")
Requires:
Conflicts:
Libs: -L\${libdir} -l${lib_name}
Libs.private: -lm
Cflags: -I\${includedir}
EOF
    fi
done

echo ""
echo "FFmpeg built successfully"
echo "  Libraries: $(find "$SYSROOT/lib" -name 'libav*.a' | wc -l)"
echo "  Headers:   $(find "$SYSROOT/include/libav*" -type f 2>/dev/null | wc -l)"
