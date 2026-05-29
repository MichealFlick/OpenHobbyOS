#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/doom"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

DOOM_SRC="$ROOT/user/lib/doomgeneric/doomgeneric"
PORT_DIR="$ROOT/ports/doom"

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

CC="$TARGET-gcc"
AR="$TARGET-ar"

CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie"
CFLAGS="$CFLAGS -I$DOOM_SRC"
CFLAGS="$CFLAGS -DNORMALUNIX -DLINUX -DSNDSERV -D_DEFAULT_SOURCE"
CFLAGS="$CFLAGS -DFILES_DIR='\"/\"'"

SRC_DOOM="\
dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main \
d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom \
i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat \
m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy \
p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg \
p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw \
r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff \
s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone \
w_file_stdc i_input i_video doomgeneric"

OBJS=""

compile() {
    local src="$1"
    local obj="$BUILD_DIR/$(basename "$src" .c).o"
    echo "  CC    $(basename "$src")"
    "$CC" -c $CFLAGS "$src" -o "$obj"
    OBJS="$OBJS $obj"
}

echo "Compiling doom sources..."
for f in $SRC_DOOM; do compile "$DOOM_SRC/$f.c"; done

echo "Compiling platform port..."
compile "$PORT_DIR/doom_port.c"

echo "  LD    doom"
"$CC" \
    --sysroot="$SYSROOT" \
    -static \
    -Wl,-T,"$SYSROOT/lib/user.ld" \
    -nostartfiles \
    -o "$BUILD_DIR/doom" \
    $OBJS \
    -Wl,--start-group \
    -lc \
    -lgcc \
    -lm \
    -Wl,--end-group \
    -lopenhobbyosgloss \
    -Wl,--allow-multiple-definition

echo "  INSTALL doom"
install -m 755 "$BUILD_DIR/doom" "$SYSROOT/bin/doom"

echo "doom built successfully"
