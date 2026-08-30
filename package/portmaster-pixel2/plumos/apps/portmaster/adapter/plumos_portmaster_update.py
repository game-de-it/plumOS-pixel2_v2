#!/usr/bin/env python3
"""Validated, staged PortMaster updater owned by plumOS."""

from __future__ import annotations

import errno
import hashlib
import json
import os
import shutil
import stat
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath
from typing import NoReturn


PLUMOS_ROOT = Path(os.environ.get("PLUMOS_ROOT", "/mnt/plumos"))
APP_ROOT = Path(
    os.environ.get(
        "PLUMOS_PORTMASTER_DATA_ROOT", PLUMOS_ROOT / "state/portmaster/data"
    )
)
RUN_ROOT = Path(os.environ.get("PLUMOS_PORTMASTER_RUN_ROOT", "/run/plumos/portmaster"))
RELEASE_INFO_URL = os.environ.get(
    "PLUMOS_PORTMASTER_RELEASE_INFO_URL",
    "https://github.com/PortsMaster/PortMaster-GUI/releases/latest/download/version.json",
)
RELEASE_BASE_URL = os.environ.get(
    "PLUMOS_PORTMASTER_RELEASE_BASE_URL",
    "https://github.com/PortsMaster/PortMaster-GUI/releases/download",
)
REQUIRED_FILES = {
    "PortMaster/pugwash",
    "PortMaster/control.txt",
    "PortMaster/device_info.txt",
    "PortMaster/funcs.txt",
    "PortMaster/gptokeyb",
    "PortMaster/gptokeyb2",
    "PortMaster/version",
}
EXECUTABLE_FILES = (
    "PortMaster/gptokeyb",
    "PortMaster/gptokeyb2",
)
EXECUTABLE_GLOBS = (
    "PortMaster/runtimes/love_*/love.aarch64",
)
FOREIGN_ADAPTER_DIRS = (
    ".Backup",
    "bato" "cera",
    "knul" "li",
    "miyoo",
    "muos",
    "retrodeck",
    "trimui",
)
FOREIGN_ADAPTER_FILES = (
    "libgl_Bato" "cera.txt",
    "libgl_Emu" "ELEC.txt",
    "libgl_JELOS.txt",
    "libgl_Miyoo.txt",
    "libgl_REGLinux.txt",
    "libgl_ROCK" "NIX.txt",
    "libgl_UnofficialOS.txt",
    "libgl_knul" "li.txt",
    "libgl_muOS.txt",
    "libgl_uConsole.txt",
    "mod_ArkOS.txt",
    "mod_ArkOS wuMMLe.txt",
    "mod_Bato" "cera.txt",
    "mod_Emu" "ELEC.txt",
    "mod_JELOS.txt",
    "mod_Miyoo.txt",
    "mod_REGLinux.txt",
    "mod_ROCK" "NIX.txt",
    "mod_TrimUI.txt",
    "mod_UnofficialOS.txt",
    "mod_dArkOS.txt",
    "mod_dArkOSRE.txt",
    "mod_knul" "li.txt",
    "mod_muOS.txt",
)
ADAPTER_VERSION = 51
STALE_UPDATE_PREFIXES = (
    "portmaster-download-",
    "upstream.next.",
)


def fail(message: str) -> NoReturn:
    raise SystemExit(f"plumos-portmaster-update: {message}")


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def sync_directory(path: Path) -> None:
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    fd = os.open(path, flags)
    try:
        try:
            os.fsync(fd)
        except OSError as error:
            if error.errno != errno.EINVAL:
                raise
            os.sync()
    finally:
        os.close(fd)


def write_json_durable(path: Path, value: dict) -> None:
    temp_path = path.with_name(f"{path.name}.tmp.{os.getpid()}")
    try:
        with temp_path.open("w", encoding="utf-8") as output:
            json.dump(value, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        temp_path.replace(path)
        sync_directory(path.parent)
    except BaseException:
        temp_path.unlink(missing_ok=True)
        raise


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": "plumOS-Pixel2-PortMaster/1"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except (urllib.error.URLError, TimeoutError, OSError, ValueError) as error:
        fail(f"release metadata download failed: {error}")


def release_record(channel: str) -> tuple[str, str, str]:
    data = fetch_json(RELEASE_INFO_URL)
    record = data.get(channel)
    if not isinstance(record, dict):
        fail(f"release channel is unavailable: {channel}")
    version = str(record.get("version", "")).strip()
    md5 = str(record.get("md5", "")).strip().lower()
    url = str(record.get("url", "")).strip()
    if not url:
        url = f"{RELEASE_BASE_URL}/{version}/PortMaster.zip"
    if not version or len(md5) != 32 or any(c not in "0123456789abcdef" for c in md5):
        fail(f"invalid release metadata for channel: {channel}")
    return version, md5, url


def installed_version() -> str:
    version_file = APP_ROOT / "upstream/PortMaster/version"
    if version_file.is_file():
        return version_file.read_text(encoding="utf-8", errors="replace").strip()
    return str(read_json(APP_ROOT / "installed.json").get("version", "")).strip()


def ensure_runtime_stopped() -> None:
    checks = (
        (RUN_ROOT / "portmaster.pid", b"plumos_portmaster_bootstrap.py", "PortMaster"),
        (RUN_ROOT / "port.pid", b"", "a PortMaster game"),
    )
    for pid_file, expected_cmdline, label in checks:
        if not pid_file.is_file():
            continue
        try:
            pid = int(pid_file.read_text(encoding="ascii").strip())
            cmdline = Path(f"/proc/{pid}/cmdline").read_bytes().replace(b"\0", b" ")
        except (OSError, ValueError):
            pid_file.unlink(missing_ok=True)
            continue
        if not expected_cmdline or expected_cmdline in cmdline:
            fail(f"{label} is running (pid={pid}); close it before updating")
        pid_file.unlink(missing_ok=True)


def cleanup_stale_update_paths() -> list[str]:
    """Remove only updater-owned temporary children from APP_ROOT."""
    if not APP_ROOT.is_dir():
        return []

    removed: list[str] = []
    for path in sorted(APP_ROOT.iterdir(), key=lambda item: item.name):
        if not path.name.startswith(STALE_UPDATE_PREFIXES):
            continue
        try:
            if path.is_symlink() or not path.is_dir():
                path.unlink()
            else:
                shutil.rmtree(path)
        except OSError as error:
            fail(f"cannot remove stale update path {path.name}: {error}")
        removed.append(path.name)

    if removed:
        sync_directory(APP_ROOT)
        print("Removed stale PortMaster update paths: " + ", ".join(removed))
    return removed


def validate_archive(archive: Path) -> None:
    with zipfile.ZipFile(archive) as zf:
        names = set()
        for entry in zf.infolist():
            path = PurePosixPath(entry.filename)
            mode = entry.external_attr >> 16
            if path.is_absolute() or ".." in path.parts or stat.S_ISLNK(mode):
                fail(f"unsafe release archive entry: {entry.filename}")
            names.add(entry.filename.rstrip("/"))
        missing = sorted(REQUIRED_FILES - names)
        if missing:
            fail("release archive is incomplete: " + ", ".join(missing))


def enable_runtime_executables(stage: Path) -> None:
    for relative in EXECUTABLE_FILES:
        path = stage / relative
        try:
            path.chmod(0o755)
        except OSError as error:
            fail(f"cannot enable runtime executable {relative}: {error}")
    for pattern in EXECUTABLE_GLOBS:
        for path in stage.glob(pattern):
            try:
                path.chmod(0o755)
            except OSError as error:
                fail(f"cannot enable runtime executable {path}: {error}")


def prune_foreign_adapters(stage: Path) -> None:
    """Keep the official common runtime but expose only the plumOS adapter."""
    portmaster = stage / "PortMaster"
    for name in FOREIGN_ADAPTER_DIRS:
        path = portmaster / name
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)
    for name in FOREIGN_ADAPTER_FILES:
        path = portmaster / name
        if path.is_symlink() or path.is_file():
            path.unlink()


def hash_file(path: Path, algorithm: str) -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": "plumOS-Pixel2-PortMaster/1"})
    try:
        with urllib.request.urlopen(request, timeout=60) as response, destination.open("wb") as output:
            shutil.copyfileobj(response, output, 1024 * 1024)
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        destination.unlink(missing_ok=True)
        fail(f"release archive download failed: {error}")


def install(channel: str, force: bool) -> None:
    ensure_runtime_stopped()
    cleanup_stale_update_paths()
    version, expected_md5, url = release_record(channel)
    current = installed_version()
    if current == version and not force:
        print(f"PortMaster is current: {version} ({channel})")
        return

    APP_ROOT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="portmaster-download-", dir=str(APP_ROOT)) as temp_dir:
        archive = Path(temp_dir) / "PortMaster.zip"
        print(f"Downloading PortMaster {version} ({channel})")
        download(url, archive)
        actual_md5 = hash_file(archive, "md5")
        if actual_md5 != expected_md5:
            fail(f"MD5 mismatch: expected {expected_md5}, got {actual_md5}")
        actual_sha256 = hash_file(archive, "sha256")
        validate_archive(archive)

        stage = APP_ROOT / f"upstream.next.{os.getpid()}"
        if stage.exists():
            shutil.rmtree(stage)
        stage.mkdir()
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(stage)
        prune_foreign_adapters(stage)
        enable_runtime_executables(stage)

        staged_version = (stage / "PortMaster/version").read_text(
            encoding="utf-8", errors="replace"
        ).strip()
        if staged_version != version:
            shutil.rmtree(stage)
            fail(f"archive version mismatch: expected {version}, got {staged_version}")

        os.sync()

        current_dir = APP_ROOT / "upstream"
        previous_dir = APP_ROOT / "upstream.previous"
        if previous_dir.exists():
            shutil.rmtree(previous_dir)
            sync_directory(APP_ROOT)
        if current_dir.exists():
            current_dir.rename(previous_dir)
            sync_directory(APP_ROOT)
        try:
            stage.rename(current_dir)
            sync_directory(APP_ROOT)
        except OSError as error:
            fail(
                "staged switch failed; current payload was preserved as "
                f"{previous_dir}: {error}"
            )

        metadata = {
            "adapter_version": ADAPTER_VERSION,
            "channel": channel,
            "official_md5": actual_md5,
            "official_sha256": actual_sha256,
            "source_url": url,
            "version": version,
        }
        write_json_durable(APP_ROOT / "installed.json", metadata)
        os.sync()
        print(f"Installed PortMaster {version} ({actual_sha256})")
        print(f"Previous payload: {previous_dir if previous_dir.exists() else 'none'}")


def status(channel: str) -> None:
    version, md5, url = release_record(channel)
    current = installed_version() or "not-installed"
    print(f"installed={current}")
    print(f"latest={version}")
    print(f"channel={channel}")
    print(f"update_available={'yes' if current != version else 'no'}")
    print(f"official_md5={md5}")
    print(f"url={url}")


def main(argv: list[str]) -> int:
    command = argv[1] if len(argv) > 1 else "status"
    channel = argv[2] if len(argv) > 2 and not argv[2].startswith("--") else "stable"
    force = "--force" in argv[2:]
    if channel not in {"stable", "beta", "alpha"}:
        fail(f"unsupported release channel: {channel}")
    if command in {"status", "check"}:
        status(channel)
    elif command == "install":
        install(channel, force)
    else:
        fail("usage: plumOS-portmaster-update [status|install] [stable|beta|alpha] [--force]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
