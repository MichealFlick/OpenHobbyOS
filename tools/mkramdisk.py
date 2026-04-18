#!/usr/bin/env python3
import os
import struct
import sys

MAGIC = b"OHOSRD1\0"
HEADER_STRUCT = struct.Struct("<8sIII")
ENTRY_STRUCT = struct.Struct("<64sIII")


def pack_path(path: str) -> bytes:
    data = path.encode("utf-8")
    if len(data) >= 64:
        raise SystemExit(f"archive path too long: {path}")
    return data + b"\0" * (64 - len(data))


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: mkramdisk.py OUTPUT SRC::DEST [...]", file=sys.stderr)
        return 1

    output = sys.argv[1]
    specs = sys.argv[2:]
    entries = []
    blobs = []

    data_offset = HEADER_STRUCT.size + ENTRY_STRUCT.size * len(specs)
    cursor = data_offset

    for spec in specs:
        src, dest = spec.split("::", 1)
        with open(src, "rb") as handle:
            blob = handle.read()
        entries.append((pack_path(dest), cursor, len(blob), 0))
        blobs.append(blob)
        cursor += len(blob)

    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "wb") as handle:
        handle.write(HEADER_STRUCT.pack(MAGIC, 1, len(entries), data_offset))
        for entry in entries:
            handle.write(ENTRY_STRUCT.pack(*entry))
        for blob in blobs:
            handle.write(blob)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
