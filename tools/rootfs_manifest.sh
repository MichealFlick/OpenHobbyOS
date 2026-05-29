#!/usr/bin/env bash
# Shared list of root filesystem files (same layout as the former packed initrd).
# Source with ROOT set to the OpenHobbyOS tree. Defines ohos_rootfs_append_entries
# which appends "SRC::/dst/path" lines to the array name passed as $1.

ohos_rootfs_append_entries() {
    local -n _out="$1"

    _out+=("$ROOT/assets/etc/motd.txt::/etc/motd.txt")
    _out+=("$ROOT/assets/etc/goshrc::/etc/goshrc")
    _out+=("$ROOT/assets/etc/os-release::/etc/os-release")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/config.jsonc::/etc/xdg/fastfetch/config.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/minimal.jsonc::/etc/xdg/fastfetch/minimal.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo.txt::/etc/xdg/fastfetch/ohos_logo.txt")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo_small.txt::/etc/xdg/fastfetch/ohos_logo_small.txt")
    _out+=("$ROOT/assets/shell-colors.jsonc::/etc/shell-colors.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/config.jsonc::/root/.config/fastfetch/config.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/minimal.jsonc::/root/.config/fastfetch/minimal.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo.txt::/root/.config/fastfetch/ohos_logo.txt")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo_small.txt::/root/.config/fastfetch/ohos_logo_small.txt")
    _out+=("$ROOT/build/user/hello.elf::/bin/hello")
    _out+=("$ROOT/build/user/uname.elf::/bin/uname")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/toolbox")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/ls")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/cat")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/stat")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/pwd")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/env")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/id")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/echo")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/sleep")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/mkdir")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/clear")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/parallel")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/yield")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/poweroff")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/reboot")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/suspend")
    _out+=("$ROOT/build/user/toolbox.elf::/bin/help")
    _out+=("$ROOT/build/user/gosh.elf::/bin/gosh")
    _out+=("$ROOT/build/user/gosh.elf::/bin/shell")
    _out+=("$ROOT/build/user/gosh.elf::/bin/sh")
    _out+=("$ROOT/build/user/test_fb.elf::/bin/test_fb")
    _out+=("$ROOT/build/user/net_test.elf::/bin/net_test")
    _out+=("$ROOT/build/user/net_info.elf::/bin/net_info")

    if [[ -f "$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch" ]]; then
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/bin/fastfetch")
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/bin/fetch")
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/usr/bin/fastfetch")
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/usr/bin/fastfetch.upstream")
    fi

    local preset
    for preset in all.jsonc neofetch.jsonc archey.jsonc screenfetch.jsonc; do
        if [[ -f "$ROOT/build/ports/fastfetch/install/usr/share/fastfetch/presets/$preset" ]]; then
            _out+=("$ROOT/build/ports/fastfetch/install/usr/share/fastfetch/presets/$preset::/usr/share/fastfetch/presets/$preset")
        fi
    done

    if [[ -f "$ROOT/build/ports/xnx/install/bin/xnx-compositor" ]]; then
        _out+=("$ROOT/build/ports/xnx/install/bin/xnx-compositor::/bin/xnx-compositor")
        _out+=("$ROOT/build/ports/xnx/install/bin/xnx-compositor::/usr/bin/xnx-compositor")
    fi

    if [[ -f "$ROOT/build/ports/xnx/install/bin/xnx-demo" ]]; then
        _out+=("$ROOT/build/ports/xnx/install/bin/xnx-demo::/bin/xnx-demo")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/doom" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/doom::/bin/doom")
    fi

    if [[ -f "$ROOT/assets/Doom1.WAD" ]]; then
        _out+=("$ROOT/assets/Doom1.WAD::/doom1.wad")
    fi
}
