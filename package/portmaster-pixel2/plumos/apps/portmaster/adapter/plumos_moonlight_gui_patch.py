#!/usr/bin/env python3
"""Apply the bounded Pixel2 compatibility patch to Moonlight New's LÖVE GUI."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import sys
from typing import NoReturn


MARKER = "-- plumOS Pixel2 Moonlight GUI adapter 52"

RUNTIME_PATHS = (
    "LD_LIBRARY_PATH=%s/moonlight/libs:/usr/lib/compat",
    "LD_LIBRARY_PATH=%s/moonlight/libs:/run/plumos/portmaster/lib:/usr/lib/compat",
)
RUNTIME_PATH = (
    "LD_LIBRARY_PATH=%s/moonlight/libs:/usr/lib/compat:${LD_LIBRARY_PATH:-}"
)

FONT_OLD = (
    "    local fontSize = 38 * scaleFactor  -- Adjust the font size based on the "
    "scale factor"
)
FONT_NEW = (
    "    local fontSize = math.max(28, 38 * scaleFactor)  -- Pixel2 readability floor"
)

APP_FILTER_OLD = "            if appName then\n                table.insert(apps, appName)"
APP_FILTER_NEW = (
    '            if appName and appName ~= "Load apps first" then\n'
    "                table.insert(apps, appName)"
)

EXEC_OLD = """    -- Construct the full command including the keydir option
    local fullCommand = string.format("LD_LIBRARY_PATH=%s/moonlight/libs:/usr/lib/compat:${LD_LIBRARY_PATH:-} %s/moonlight/%s %s > %s 2>&1 &", currentDir, currentDir, command, keydirOption, outputFileName)

    -- Execute the command in the background
    os.execute(fullCommand)
"""
EXEC_NEW = """    -- Pairing must stay interactive, while app listing must finish before reload.
    local runInBackground = command:match("^moonlight pair ") ~= nil
    local backgroundSuffix = runInBackground and " &" or ""
    local fullCommand = string.format("LD_LIBRARY_PATH=%s/moonlight/libs:/usr/lib/compat:${LD_LIBRARY_PATH:-} %s/moonlight/%s %s > %s 2>&1%s", currentDir, currentDir, command, keydirOption, outputFileName, backgroundSuffix)

    os.execute(fullCommand)
"""


def fail(message: str) -> NoReturn:
    raise SystemExit(f"plumos-moonlight-gui-patch: {message}")


def replace_known(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    count = text.count(old)
    if count != 1:
        fail(f"unknown {label} contract (matches={count})")
    return text.replace(old, new, 1)


def patch_text(text: str) -> str:
    if RUNTIME_PATH not in text:
        matches = [value for value in RUNTIME_PATHS if value in text]
        if len(matches) != 1:
            fail(f"unknown runtime-path contract (matches={len(matches)})")
        text = text.replace(matches[0], RUNTIME_PATH, 1)

    text = replace_known(text, FONT_OLD, FONT_NEW, "font-size")
    text = replace_known(text, APP_FILTER_OLD, APP_FILTER_NEW, "app-filter")
    text = replace_known(text, EXEC_OLD, EXEC_NEW, "app-list execution")

    if MARKER not in text:
        text = f"{MARKER}\n{text}"
    return text


def main(argv: list[str]) -> int:
    if len(argv) not in (2, 3):
        fail("usage: PATCHER MAIN.LUA [BACKUP-DIR]")

    target = Path(argv[1])
    try:
        original = target.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"cannot read {target}: {exc}")

    patched = patch_text(original)
    if patched == original:
        return 0

    if len(argv) == 3:
        backup_dir = Path(argv[2])
        digest = hashlib.sha256(original.encode("utf-8")).hexdigest()
        backup = backup_dir / f"main-{digest}.lua"
        try:
            backup_dir.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                shutil.copy2(target, backup)
        except OSError as exc:
            fail(f"cannot preserve pre-patch GUI: {exc}")

    temporary = target.with_name(f".{target.name}.plumos-{os.getpid()}")
    try:
        temporary.write_text(patched, encoding="utf-8")
        os.chmod(temporary, target.stat().st_mode)
        os.replace(temporary, target)
        directory_fd = os.open(target.parent, os.O_RDONLY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    except OSError as exc:
        try:
            temporary.unlink()
        except OSError:
            pass
        fail(f"cannot install patched GUI: {exc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
