#!/usr/bin/env python3
"""Generate native Pixel2 framebuffer frames for boot-time updates."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


LOGICAL_WIDTH = 640
LOGICAL_HEIGHT = 480
PHYSICAL_WIDTH = 480
PHYSICAL_HEIGHT = 640

FONT = {
    " ": ["00000"] * 7,
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01111", "10000", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
}


def color(value: int) -> bytes:
    return struct.pack("<I", value)


def fill_rect(frame: bytearray, x: int, y: int, width: int, height: int, value: int) -> None:
    x0 = max(0, x)
    x1 = min(LOGICAL_WIDTH, x + width)
    if x1 <= x0:
        return
    row = color(value) * (x1 - x0)
    for py in range(max(0, y), min(LOGICAL_HEIGHT, y + height)):
        start = (py * LOGICAL_WIDTH + x0) * 4
        frame[start : start + len(row)] = row


def draw_text(frame: bytearray, text: str, y: int, scale: int, value: int) -> None:
    text = text.upper()
    glyph_width = 6 * scale
    x = (LOGICAL_WIDTH - max(0, len(text) * glyph_width - scale)) // 2
    for char in text:
        for row_index, row in enumerate(FONT.get(char, FONT[" "])):
            for column, enabled in enumerate(row):
                if enabled == "1":
                    fill_rect(
                        frame,
                        x + column * scale,
                        y + row_index * scale,
                        scale,
                        scale,
                        value,
                    )
        x += glyph_width


def rotate_ccw(logical: bytearray) -> bytes:
    physical = bytearray(PHYSICAL_WIDTH * PHYSICAL_HEIGHT * 4)
    for y in range(LOGICAL_HEIGHT):
        for x in range(LOGICAL_WIDTH):
            source = (y * LOGICAL_WIDTH + x) * 4
            destination_x = y
            destination_y = LOGICAL_WIDTH - 1 - x
            destination = (destination_y * PHYSICAL_WIDTH + destination_x) * 4
            physical[destination : destination + 4] = logical[source : source + 4]
    return bytes(physical)


def render(message: str, percent: int, error: bool = False) -> bytes:
    background = 0xFF071014
    panel = 0xFF152329
    text = 0xFFE8F1F2
    muted = 0xFF8FA6AA
    accent = 0xFFE34A4A if error else 0xFFFF8A00
    frame = bytearray(color(background) * (LOGICAL_WIDTH * LOGICAL_HEIGHT))

    fill_rect(frame, 0, 0, LOGICAL_WIDTH, 8, accent)
    draw_text(frame, "PLUMOS PIXEL2", 68, 5, text)
    fill_rect(frame, 56, 150, LOGICAL_WIDTH - 112, 180, panel)
    draw_text(frame, message, 205, 4, text)
    draw_text(frame, "CHECK LOGS" if error else "DO NOT POWER OFF", 270, 3, muted)
    fill_rect(frame, 80, 370, 480, 28, 0xFF293B40)
    fill_rect(frame, 84, 374, int(472 * percent / 100), 20, accent)
    return rotate_ccw(frame)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    stages = {
        "update_verify": ("VERIFYING UPDATE", 15, False),
        "update_runtime": ("UPDATING RUNTIME", 45, False),
        "update_system": ("UPDATING SYSTEM", 55, False),
        "update_finalize": ("FINALIZING UPDATE", 90, False),
        "update_rollback": ("RESTORING PREVIOUS", 70, False),
        "update_error": ("UPDATE FAILED", 100, True),
    }
    expected_size = PHYSICAL_WIDTH * PHYSICAL_HEIGHT * 4
    for name, (message, percent, error) in stages.items():
        payload = render(message, percent, error)
        if len(payload) != expected_size:
            raise RuntimeError(f"unexpected frame size for {name}: {len(payload)}")
        (output_dir / f"{name}.raw").write_bytes(payload)


if __name__ == "__main__":
    main()
