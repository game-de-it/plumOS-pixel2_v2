#!/usr/bin/env python3
"""Reject any Pixel2 runtime-DTB change except the DWC2 VBUS supply link."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stock-dts", type=Path, required=True)
    parser.add_argument("--patched-dts", type=Path, required=True)
    parser.add_argument("--phandle", required=True)
    args = parser.parse_args()

    stock = args.stock_dts.read_text(encoding="utf-8")
    patched = args.patched_dts.read_text(encoding="utf-8")
    phandle = args.phandle.lower().removeprefix("0x")
    target = re.compile(
        rf"^[ \t]*vbus-supply = <0x0*{re.escape(phandle)}>;\n",
        re.MULTILINE,
    )
    matches = target.findall(patched)
    if len(matches) != 1:
        raise SystemExit(
            f"error: expected one vbus-supply property for phandle 0x{phandle}, "
            f"found {len(matches)}"
        )
    if "vbus-supply" in stock:
        raise SystemExit("error: stock Pixel2 DTB unexpectedly contains vbus-supply")
    restored = target.sub("", patched, count=1)
    if restored != stock:
        raise SystemExit("error: patched Pixel2 DTB changes more than vbus-supply")
    print(f"pixel2_boot_dtb_diff=result-ok property=vbus-supply phandle=0x{phandle}")


if __name__ == "__main__":
    main()
