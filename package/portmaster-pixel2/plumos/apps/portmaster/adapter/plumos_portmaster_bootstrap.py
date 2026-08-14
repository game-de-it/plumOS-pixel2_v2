#!/usr/bin/env python3
"""Start the official PortMaster GUI with the plumOS Pixel2 hardware contract."""

from __future__ import annotations

import builtins
import hashlib
import os
import shutil
import stat
import sys
import zipfile
from pathlib import Path, PurePosixPath


APP_ROOT = Path(
    os.environ.get(
        "PLUMOS_PORTMASTER_APP_ROOT", "/mnt/plumos/apps/portmaster"
    )
)
DATA_ROOT = Path(
    os.environ.get(
        "PLUMOS_PORTMASTER_DATA_ROOT",
        "/mnt/plumos/state/portmaster/data",
    )
)
PORTMASTER_DIR = DATA_ROOT / "upstream" / "PortMaster"
SELF_UPDATE_CALL = b"            if portmaster_check_update(pm, config, temp_dir):"
PLUMOS_SELF_UPDATE_CALL = b"            if plumos_portmaster_check_update(pm, config, temp_dir):"


def canonicalize_existing_item_case(root: Path, expected_name: str) -> bool:
    """Rename one case-only VFAT conflict to the spelling required by a port."""
    expected = root / expected_name
    if not root.is_dir():
        return False

    matches = [
        candidate
        for candidate in root.iterdir()
        if candidate.name.casefold() == expected_name.casefold()
    ]
    if any(candidate.name == expected_name for candidate in matches):
        return False
    if len(matches) != 1:
        return False

    current = matches[0]
    temporary = root / f".plumos-casefix-{os.getpid()}-{expected_name}"
    suffix = 0
    while temporary.exists():
        suffix += 1
        temporary = root / (
            f".plumos-casefix-{os.getpid()}-{suffix}-{expected_name}"
        )

    current.rename(temporary)
    try:
        temporary.rename(expected)
    except Exception:
        temporary.rename(current)
        raise
    return True


def port_archive_top_level_items(archive: Path) -> tuple[tuple[str, bool], ...]:
    """Return safe top-level port directories and scripts without reading payloads."""
    items: dict[tuple[str, bool], None] = {}
    with zipfile.ZipFile(archive) as zf:
        for entry in zf.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or ".." in path.parts or not path.parts:
                return ()

            if len(path.parts) > 1:
                item = (path.parts[0], False)
            elif path.name.casefold().endswith(".sh"):
                item = (path.name, True)
            else:
                continue
            items.setdefault(item, None)
    return tuple(items)


def install_port_case_normalizer(harbourmaster_module) -> None:
    """Preserve restored user data when archive spelling differs only by case."""
    harbourmaster_class = harbourmaster_module.HarbourMaster
    original = harbourmaster_class._install_port
    if getattr(original, "_plumos_case_normalizer", False):
        return

    def plumos_install_port(self, download_info, do_delete=False):
        archive = download_info.get("zip_file")
        if isinstance(archive, (str, os.PathLike)):
            archive = Path(archive)
        if isinstance(archive, Path) and archive.is_file():
            for expected_name, is_script in port_archive_top_level_items(archive):
                root = self.scripts_dir if is_script else self.ports_dir
                if canonicalize_existing_item_case(root, expected_name):
                    print(
                        "plumOS PortMaster: normalized existing item "
                        f"to {expected_name!r}"
                    )
        return original(self, download_info, do_delete=do_delete)

    plumos_install_port._plumos_case_normalizer = True
    harbourmaster_class._install_port = plumos_install_port


def safe_extract_pylibs() -> None:
    archive = PORTMASTER_DIR / "pylibs.zip"
    if not archive.is_file():
        return

    with zipfile.ZipFile(archive) as zf:
        for entry in zf.infolist():
            path = PurePosixPath(entry.filename)
            mode = entry.external_attr >> 16
            if path.is_absolute() or ".." in path.parts or stat.S_ISLNK(mode):
                raise RuntimeError(f"unsafe pylibs entry: {entry.filename}")

        for name in ("pylibs", "exlibs"):
            target = PORTMASTER_DIR / name
            if target.exists():
                shutil.rmtree(target)
        zf.extractall(PORTMASTER_DIR)
        os.sync()

    digest = hashlib.md5(archive.read_bytes()).hexdigest()
    digest_path = PORTMASTER_DIR / "pylibs.zip.md5"
    temp_digest = digest_path.with_name(f"{digest_path.name}.tmp.{os.getpid()}")
    with temp_digest.open("w", encoding="ascii") as output:
        output.write(digest + "\n")
        output.flush()
        os.fsync(output.fileno())
    temp_digest.replace(digest_path)
    archive.unlink()
    os.sync()


def install_pixel2_contract() -> None:
    sys.path.insert(0, str(PORTMASTER_DIR / "exlibs"))
    sys.path.insert(0, str(PORTMASTER_DIR / "pylibs"))

    import harbourmaster  # type: ignore
    from harbourmaster import hardware, platform  # type: ignore

    hardware.DEVICES["GKD Pixel2"] = {
        "device": "gkd-pixel2",
        "manufacturer": "Game Kiddy",
        "cfw": ["plumOS"],
    }
    hardware.HW_INFO["gkd-pixel2"] = {
        "resolution": (640, 480),
        "analogsticks": 0,
        "cpu": "rk3326",
        "capabilities": [],
        "ram": 1024,
    }

    original_new_device_info = hardware.new_device_info

    def plumos_new_device_info():
        info = original_new_device_info()
        info.update(
            name="plumOS",
            version=os.environ.get("PLUMOS_PORTMASTER_CFW_VERSION", "unknown"),
            device="gkd-pixel2",
        )
        return info

    hardware.new_device_info = plumos_new_device_info
    hardware.__root_info = None
    harbourmaster.HW_INFO["gkd-pixel2"] = hardware.HW_INFO["gkd-pixel2"]
    platform.HM_PLATFORMS["plumos"] = platform.PlatformBase
    harbourmaster.HM_PLATFORMS["plumos"] = platform.PlatformBase
    install_port_case_normalizer(harbourmaster)


def disable_upstream_self_update(source: bytes) -> bytes:
    """Keep catalog checks enabled while plumOS owns payload replacement."""
    if source.count(SELF_UPDATE_CALL) != 1:
        raise RuntimeError("unsupported PortMaster self-update call layout")
    return source.replace(SELF_UPDATE_CALL, PLUMOS_SELF_UPDATE_CALL, 1)


def plumos_portmaster_check_update(*_args, **_kwargs) -> bool:
    return False


def main() -> int:
    if not (PORTMASTER_DIR / "pugwash").is_file():
        raise SystemExit(f"PortMaster payload is incomplete: {PORTMASTER_DIR}")

    safe_extract_pylibs()
    install_pixel2_contract()
    pugwash = PORTMASTER_DIR / "pugwash"
    sys.argv[0] = str(pugwash)
    globals_dict = {
        "__builtins__": builtins,
        "__file__": str(pugwash),
        "__name__": "__main__",
        "__package__": None,
        "plumos_portmaster_check_update": plumos_portmaster_check_update,
    }
    source = disable_upstream_self_update(pugwash.read_bytes())
    exec(compile(source, str(pugwash), "exec"), globals_dict)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
