#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:?missing output path}

entries=(
    "$ROOT/assets/etc/motd.txt::/etc/motd.txt"
    "$ROOT/assets/etc/os-release::/etc/os-release"
    "$ROOT/assets/etc/xdg/fastfetch/config.jsonc::/etc/xdg/fastfetch/config.jsonc"
    "$ROOT/assets/etc/xdg/fastfetch/minimal.jsonc::/etc/xdg/fastfetch/minimal.jsonc"
    "$ROOT/assets/etc/xdg/fastfetch/config.jsonc::/root/.config/fastfetch/config.jsonc"
    "$ROOT/assets/etc/xdg/fastfetch/minimal.jsonc::/root/.config/fastfetch/minimal.jsonc"
    "$ROOT/build/user/hello.elf::/bin/hello"
    "$ROOT/build/user/uname.elf::/bin/uname"
    "$ROOT/build/user/toolbox.elf::/bin/toolbox"
    "$ROOT/build/user/toolbox.elf::/bin/ls"
    "$ROOT/build/user/toolbox.elf::/bin/cat"
    "$ROOT/build/user/toolbox.elf::/bin/stat"
    "$ROOT/build/user/toolbox.elf::/bin/pwd"
    "$ROOT/build/user/toolbox.elf::/bin/env"
    "$ROOT/build/user/toolbox.elf::/bin/id"
    "$ROOT/build/user/toolbox.elf::/bin/echo"
    "$ROOT/build/user/toolbox.elf::/bin/sleep"
)

if [[ -f "$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch" ]]; then
    entries+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/bin/fastfetch")
    entries+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/bin/fetch")
fi

for preset in all.jsonc neofetch.jsonc archey.jsonc screenfetch.jsonc; do
    if [[ -f "$ROOT/build/ports/fastfetch/install/usr/share/fastfetch/presets/$preset" ]]; then
        entries+=("$ROOT/build/ports/fastfetch/install/usr/share/fastfetch/presets/$preset::/usr/share/fastfetch/presets/$preset")
    fi
done

if [[ -f "$ROOT/build/ports/xserver/install/usr/bin/Xvfb" ]]; then
    entries+=("$ROOT/build/ports/xserver/install/usr/bin/Xvfb::/bin/Xvfb")
fi

for asset in rgb.txt XErrorDB; do
    if [[ -f "$ROOT/build/ports/xserver/install/usr/share/X11/$asset" ]]; then
        entries+=("$ROOT/build/ports/xserver/install/usr/share/X11/$asset::/usr/share/X11/$asset")
    fi
done

if [[ -d "$ROOT/build/ports/xserver/install/usr/share/X11/xkb" ]]; then
    while IFS= read -r file; do
        archive_path="/usr/share/X11/xkb/${file#"$ROOT/build/ports/xserver/install/usr/share/X11/xkb/"}"
        entries+=("$file::$archive_path")
    done < <(find "$ROOT/build/ports/xserver/install/usr/share/X11/xkb" -type f | sort)
fi

exec python3 "$ROOT/tools/mkramdisk.py" "$OUT" "${entries[@]}"
