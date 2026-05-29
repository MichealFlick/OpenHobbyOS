#!/usr/bin/env bash
set -euo pipefail

# Full userland archive for the multiboot module (same paths as tools/populate_disk.sh).
# With an ext2 disk, vfs merges these as an overlay for anything missing on disk.

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:?missing output path}

# shellcheck source=rootfs_manifest.sh
source "$ROOT/tools/rootfs_manifest.sh"

entries=()
ohos_rootfs_append_entries entries

exec python3 "$ROOT/tools/mkramdisk.py" "$OUT" "${entries[@]}"
