#!/bin/bash
# Prefer tools/populate_disk.sh — it creates the disk (if needed) and installs the full rootfs.
set -e

DISK_SIZE_MB=${1:-64}
DISK_IMAGE="${2:-disk.img}"
MOUNT_POINT="/tmp/ohos_disk_mount"

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (required for mounting)"
    exit 1
fi

echo "Creating ${DISK_SIZE_MB}MB disk image: ${DISK_IMAGE}"

rm -f "${DISK_IMAGE}"
dd if=/dev/zero of="${DISK_IMAGE}" bs=1M count=${DISK_SIZE_MB} status=progress

echo "Creating MBR partition table with Linux partition..."
parted -s "${DISK_IMAGE}" mklabel msdos
parted -s "${DISK_IMAGE}" mkpart primary ext2 1MiB 100%
parted -s "${DISK_IMAGE}" set 1 boot off

echo "Setting up loop device..."
LOOP_DEV=$(losetup -fP --show "${DISK_IMAGE}")
echo "Loop device: ${LOOP_DEV}"

echo "Creating ext2 filesystem..."
mkfs.ext2 "${LOOP_DEV}p1"

echo "Mounting filesystem..."
mkdir -p "${MOUNT_POINT}"
mount "${LOOP_DEV}p1" "${MOUNT_POINT}"

echo "Creating basic directory structure..."
mkdir -p "${MOUNT_POINT}/bin"
mkdir -p "${MOUNT_POINT}/dev"
mkdir -p "${MOUNT_POINT}/etc"
mkdir -p "${MOUNT_POINT}/home"
mkdir -p "${MOUNT_POINT}/lib"
mkdir -p "${MOUNT_POINT}/mnt"
mkdir -p "${MOUNT_POINT}/proc"
mkdir -p "${MOUNT_POINT}/root"
mkdir -p "${MOUNT_POINT}/sbin"
mkdir -p "${MOUNT_POINT}/tmp"
mkdir -p "${MOUNT_POINT}/usr"
mkdir -p "${MOUNT_POINT}/var"

echo "Creating basic files..."
echo "OpenHobbyOS Root Filesystem" > "${MOUNT_POINT}/etc/motd"
echo "localhost" > "${MOUNT_POINT}/etc/hostname"

echo "Unmounting..."
umount "${MOUNT_POINT}"

losetup -d "${LOOP_DEV}"
rmdir "${MOUNT_POINT}" 2>/dev/null || true

echo "Disk image created successfully: ${DISK_IMAGE}"
echo ""
echo "To use with QEMU:"
echo "  qemu-system-i386 -hda ${DISK_IMAGE} -cdrom build/OpenHobbyOS.iso ..."
