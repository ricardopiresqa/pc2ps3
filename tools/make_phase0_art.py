#!/usr/bin/env python3
"""Original geometric Phase 0 package assets. SPDX-License-Identifier: CC0-1.0"""
from pathlib import Path
import struct
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def image(width, height):
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            color = (43, 133, 171) if width // 8 < x < width * 7 // 8 and height * 3 // 8 < y < height * 5 // 8 else (24, 32, 56)
            row.extend(color)
        rows.append(row)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


if __name__ == "__main__":
    target = Path(__file__).resolve().parents[1] / "assets/phase0"
    target.mkdir(parents=True, exist_ok=True)
    for name, width, height in (("ICON0.PNG", 320, 176), ("PIC1.PNG", 1920, 1080)):
        (target / name).write_bytes(image(width, height))
