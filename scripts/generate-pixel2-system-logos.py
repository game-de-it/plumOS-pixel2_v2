#!/usr/bin/env python3
"""Generate the small plumOS Pixel2 text-badge system logos deterministically."""

from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

WIDTH = 190
HEIGHT = 156
SCALE = 3
BG = (37, 21, 47)
BORDER = (217, 106, 167)
FG = (255, 255, 255)

FONT = {
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01111", "10000", "10000", "10000", "10000", "10000", "01111"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "G": ("01111", "10000", "10000", "10111", "10001", "10001", "01111"),
    "I": ("11111", "00100", "00100", "00100", "00100", "00100", "11111"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
    "W": ("10001", "10001", "10001", "10101", "10101", "10101", "01010"),
    "Y": ("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
    "Z": ("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
}

LOGOS = {
    "arduboy": ("ARDUBOY",),
    "megaduck": ("MEGA", "DUCK"),
    "puzzlescript": ("PUZZLE", "SCRIPT"),
    "superbroswar": ("SUPER", "BROS WAR"),
}


def set_pixel(image: bytearray, x: int, y: int, color: tuple[int, int, int]) -> None:
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        offset = (y * WIDTH + x) * 3
        image[offset : offset + 3] = bytes(color)


def fill_rect(
    image: bytearray, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]
) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
            set_pixel(image, x, y, color)


def text_width(text: str) -> int:
    return len(text) * 5 * SCALE + max(0, len(text) - 1) * SCALE


def draw_text(image: bytearray, text: str, y: int) -> None:
    x = (WIDTH - text_width(text)) // 2
    for char in text:
        if char == " ":
            x += 4 * SCALE
            continue
        glyph = FONT[char]
        for row, bits in enumerate(glyph):
            for column, bit in enumerate(bits):
                if bit == "1":
                    fill_rect(
                        image,
                        x + column * SCALE,
                        y + row * SCALE,
                        x + (column + 1) * SCALE,
                        y + (row + 1) * SCALE,
                        FG,
                    )
        x += 6 * SCALE


def png_bytes(image: bytearray) -> bytes:
    scanlines = b"".join(
        b"\x00" + bytes(image[y * WIDTH * 3 : (y + 1) * WIDTH * 3])
        for y in range(HEIGHT)
    )

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + kind
            + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
        )

    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scanlines, 9))
        + chunk(b"IEND", b"")
    )


def render(lines: tuple[str, ...]) -> bytes:
    image = bytearray(BG * (WIDTH * HEIGHT))
    fill_rect(image, 4, 4, WIDTH - 4, 8, BORDER)
    fill_rect(image, 4, HEIGHT - 8, WIDTH - 4, HEIGHT - 4, BORDER)
    fill_rect(image, 4, 8, 8, HEIGHT - 8, BORDER)
    fill_rect(image, WIDTH - 8, 8, WIDTH - 4, HEIGHT - 8, BORDER)
    line_height = 7 * SCALE
    gap = 13
    total_height = len(lines) * line_height + max(0, len(lines) - 1) * gap
    y = (HEIGHT - total_height) // 2
    for line in lines:
        draw_text(image, line, y)
        y += line_height + gap
    return png_bytes(image)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT_DIR", file=sys.stderr)
        return 2
    output_dir = Path(sys.argv[1])
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, lines in LOGOS.items():
        (output_dir / f"{name}.png").write_bytes(render(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
