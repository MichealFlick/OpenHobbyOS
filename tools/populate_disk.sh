#!/usr/bin/env bash
# Create (if missing) an MBR disk image with ext2, then install the full rootfs.
# Must run as root. Set OPENHOBBYOS_ROOT to the repo root if needed.
set -euo pipefail

if [[ "${EUID:-0}" -ne 0 ]]; then
    echo "populate_disk.sh: run as root (required for loop mount)" >&2
    exit 1
fi

ROOT=${OPENHOBBYOS_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
ROOT=$(cd "$ROOT" && pwd)
DISK_ARG=${1:?"usage: populate_disk.sh /path/to/disk.img"}
DISK_IMAGE=$(cd "$(dirname "$DISK_ARG")" && pwd)/$(basename "$DISK_ARG")
DISK_SIZE_MB=${DISK_SIZE_MB:-64}
MOUNT_POINT=${MOUNT_POINT:-$(mktemp -d /tmp/ohos_rootfs_mount.XXXXXX)}

# shellcheck source=rootfs_manifest.sh
source "$ROOT/tools/rootfs_manifest.sh"

if [[ ! -f "$ROOT/tools/mkramdisk.py" ]]; then
    echo "populate_disk.sh: bad ROOT: $ROOT" >&2
    exit 1
fi

create_blank_disk() {
    echo "Creating ${DISK_SIZE_MB}MB disk: ${DISK_IMAGE}"
    dd if=/dev/zero of="${DISK_IMAGE}" bs=1M count="${DISK_SIZE_MB}" status=progress
    parted -s "${DISK_IMAGE}" mklabel msdos
    parted -s "${DISK_IMAGE}" mkpart primary ext2 1MiB 100%
    parted -s "${DISK_IMAGE}" set 1 boot off
}

if [[ ! -f "$DISK_IMAGE" ]]; then
    create_blank_disk
elif [[ "${FORCE_CREATE_DISK:-0}" == "1" ]]; then
    rm -f "${DISK_IMAGE}"
    create_blank_disk
fi

LOOP_DEV=$(losetup -fP --show "${DISK_IMAGE}")

cleanup() {
    if [[ -n "${MOUNT_POINT:-}" ]] && mountpoint -q "${MOUNT_POINT}" 2>/dev/null; then
        umount "${MOUNT_POINT}" || true
    fi
    if [[ -n "${LOOP_DEV:-}" ]]; then
        losetup -d "${LOOP_DEV}" 2>/dev/null || true
    fi
    [[ -n "${MOUNT_POINT:-}" ]] && rmdir "${MOUNT_POINT}" 2>/dev/null || true
}
trap cleanup EXIT

need_mkfs=true
if command -v blkid >/dev/null 2>&1; then
    if blkid -s TYPE -o value "${LOOP_DEV}p1" 2>/dev/null | grep -q ext2; then
        need_mkfs=false
    fi
fi
if $need_mkfs; then
    echo "Creating ext2 on ${LOOP_DEV}p1 ..."
    mkfs.ext2 "${LOOP_DEV}p1"
fi

mkdir -p "${MOUNT_POINT}"
mount "${LOOP_DEV}p1" "${MOUNT_POINT}"

mkdir -p "${MOUNT_POINT}/bin" "${MOUNT_POINT}/dev" "${MOUNT_POINT}/etc" "${MOUNT_POINT}/home" \
    "${MOUNT_POINT}/lib" "${MOUNT_POINT}/mnt" "${MOUNT_POINT}/proc" "${MOUNT_POINT}/root" \
    "${MOUNT_POINT}/sbin" "${MOUNT_POINT}/tmp" "${MOUNT_POINT}/usr" "${MOUNT_POINT}/var"

echo "localhost" > "${MOUNT_POINT}/etc/hostname"

entries=()
ohos_rootfs_append_entries entries

install_pair() {
    local spec=$1
    local src dest mode
    src=${spec%%::*}
    dest=${spec#*::}
    if [[ ! -f "$src" ]]; then
        echo "populate_disk.sh: skip missing: $src" >&2
        return 0
    fi
    case "$dest" in
        /bin/*|/usr/bin/*|/sbin/*) mode=755 ;;
        *) mode=644 ;;
    esac
    mkdir -p "$(dirname "${MOUNT_POINT}${dest}")"
    install -m"$mode" "$src" "${MOUNT_POINT}${dest}"
}

for spec in "${entries[@]}"; do
    install_pair "$spec"
done

echo "Root filesystem installed on ${DISK_IMAGE} (${#entries[@]} manifest paths)."
sync
umount "${MOUNT_POINT}"
trap - EXIT
losetup -d "${LOOP_DEV}"
LOOP_DEV=""
rmdir "${MOUNT_POINT}" 2>/dev/null || true
