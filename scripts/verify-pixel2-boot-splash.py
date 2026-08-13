#!/usr/bin/env python3
"""Validate the PNG contract consumed by the Pixel2 stock initramfs."""

from __future__ import annotations

import struct
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"pixel2-boot-splash: FAIL: {message}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: verify-pixel2-boot-splash.py PATH")

    path = Path(sys.argv[1])
    try:
        data = path.read_bytes()
    except OSError as error:
        fail(str(error))

    if len(data) < 33 or data[:8] != b"\x89PNG\r\n\x1a\n":
        fail("not a PNG file")
    if data[12:16] != b"IHDR":
        fail("IHDR is not the first PNG chunk")

    width, height, bit_depth, color_type, compression, filtering, interlace = (
        struct.unpack(">IIBBBBB", data[16:29])
    )
    if (width, height) != (480, 640):
        fail(f"geometry must be 480x640, got {width}x{height}")
    if (bit_depth, color_type) != (8, 2):
        fail(
            "image must be 8-bit RGB without alpha, "
            f"got bit_depth={bit_depth} color_type={color_type}"
        )
    if (compression, filtering, interlace) != (0, 0, 0):
        fail("unsupported PNG compression, filtering, or interlace method")
    if b"IEND" not in data[-32:]:
        fail("PNG has no terminal IEND chunk")

    print(
        f"pixel2-boot-splash: PASS path={path} size={len(data)} "
        f"geometry={width}x{height} format=png-rgb8"
    )


if __name__ == "__main__":
    main()
