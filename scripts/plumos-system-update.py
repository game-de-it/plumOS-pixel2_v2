#!/usr/bin/env python3
"""Transactional Runtime and A/B System updater for plumOS Pixel2."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import time
from typing import Any


PLUMOS_ROOT = Path(os.environ.get("PLUMOS_ROOT", "/mnt/plumos"))
USER_ROOT = Path(os.environ.get("PLUMOS_USERDATA_ROOT", "/mnt/plumos-user"))
BOOT_ROOT = Path(os.environ.get("PLUMOS_BOOT_ROOT", "/flash"))
STATE_ROOT = PLUMOS_ROOT / "update-state"
STAGING_ROOT = PLUMOS_ROOT / "updates" / "staging"
BACKUP_ROOT = PLUMOS_ROOT / "backups" / "update-previous"
REQUEST_FILE = STATE_ROOT / "request.json"
JOURNAL_FILE = STATE_ROOT / "runtime-transaction.json"
RUNTIME_PENDING = STATE_ROOT / "runtime-pending.json"
SYSTEM_ACTIVE = STATE_ROOT / "system-active"
SYSTEM_PENDING = STATE_ROOT / "system-pending"
SYSTEM_ATTEMPTED = STATE_ROOT / "system-pending-attempted"
LAST_RESULT = STATE_ROOT / "last-result.json"
PUBLIC_KEY = Path(os.environ.get("PLUMOS_UPDATE_PUBLIC_KEY", "/etc/plumos-update-public.pem"))
LOCK_FILE = Path(os.environ.get(
    "PLUMOS_UPDATE_LOCK_FILE", "/run/plumos-system-update.lock"
))
SYSTEM_ABI_FILE = Path(os.environ.get("PLUMOS_SYSTEM_ABI_FILE", "/etc/plumos-system-abi"))
SYSTEM_VERSION_FILE = Path(os.environ.get("PLUMOS_SYSTEM_VERSION_FILE", "/etc/plumos-system-version"))
RUNTIME_ABI_FILE = PLUMOS_ROOT / "RUNTIME_ABI"
DEVICE_ID = "pixel2"
ARCHITECTURE = "aarch64"
VENDOR_RUNTIME = "pixel2-rockchip-r1"
METADATA_LAST = {"VERSION", "manifest.json", "checksums.sha256"}
MANAGED_ROOTS = {
    "apps", "bin", "components", "cores", "emulator", "factory-defaults",
    "fonts", "frontend", "info", "lib", "licenses", "network", "picoarch",
    "scraper", "share", "ssh", "standalone", "themes",
}
MANAGED_ROOT_FILES = {"COMPAT_VENDOR", "RUNTIME_ABI", *METADATA_LAST}
MANAGED_CONFIG_PREFIXES = (
    "config/frontend/apps.json",
    "config/frontend/feature-contract.json",
    "config/frontend/menus.json",
    "config/frontend/scraper-sources.tsv",
    "config/frontend/systems.json",
    "config/frontend/themes.json",
    "config/standalone/",
    "config/system/input-map.env",
    "config/system/input-map.json",
)
PERSISTENT_PREFIXES = (
    "apps/portmaster/installed.json",
    "apps/portmaster/upstream/",
    "apps/portmaster/upstream.previous/",
)


class UpdateError(RuntimeError):
    pass


@contextmanager
def update_lock():
    LOCK_FILE.parent.mkdir(parents=True, exist_ok=True)
    with LOCK_FILE.open("a+", encoding="ascii") as handle:
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise UpdateError("another update operation is already running") from exc
        try:
            yield
        finally:
            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


def show_progress(stage: str) -> None:
    progress_root = Path(os.environ.get(
        "PLUMOS_UPDATE_PROGRESS_ROOT", "/usr/share/plumos/update-progress"
    ))
    framebuffer = Path(os.environ.get("PLUMOS_UPDATE_FRAMEBUFFER", "/dev/fb0"))
    frame = progress_root / f"{stage}.raw"
    if os.environ.get("PLUMOS_UPDATE_PROGRESS", "1") == "0":
        return
    try:
        with frame.open("rb") as source, framebuffer.open("wb", buffering=0) as target:
            shutil.copyfileobj(source, target, 1024 * 1024)
    except OSError:
        pass


def now() -> int:
    return int(time.time())


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_text(path: Path, default: str = "") -> str:
    try:
        return path.read_text(encoding="utf-8").strip()
    except OSError:
        return default


def fsync_directory(path: Path) -> None:
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
    except OSError:
        os.sync()
        return
    try:
        os.fsync(fd)
    except OSError:
        os.sync()
    finally:
        os.close(fd)


def fsync_file_descriptor(fd: int) -> None:
    try:
        os.fsync(fd)
    except OSError as exc:
        unsupported = {
            0,
            errno.EINVAL,
            getattr(errno, "ENOTSUP", errno.EINVAL),
            getattr(errno, "EOPNOTSUPP", errno.EINVAL),
        }
        if exc.errno not in unsupported:
            raise
        # The vendor 4.9 VFAT driver may return failure with errno left at 0
        # after flushing a large file. Complete a filesystem-wide sync and let
        # the mandatory readback SHA-256 decide whether the write is valid.
        os.sync()


def atomic_bytes(path: Path, payload: bytes, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(f".{path.name}.new-{os.getpid()}")
    with temp.open("wb") as handle:
        handle.write(payload)
        handle.flush()
        fsync_file_descriptor(handle.fileno())
    os.chmod(temp, mode)
    os.replace(temp, path)
    fsync_directory(path.parent)


def atomic_text(path: Path, text: str, mode: int = 0o644) -> None:
    atomic_bytes(path, text.encode("utf-8"), mode)


def atomic_json(path: Path, value: dict[str, Any]) -> None:
    atomic_bytes(path, (json.dumps(value, sort_keys=True, indent=2) + "\n").encode())


def remove_path(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink(missing_ok=True)
    elif path.is_dir():
        shutil.rmtree(path)


def safe_relative(value: str) -> str:
    pure = PurePosixPath(value)
    if not value or pure.is_absolute() or ".." in pure.parts or str(pure) != value:
        raise UpdateError(f"unsafe payload path: {value}")
    return value


def managed_runtime_path(relative: str) -> bool:
    if relative in MANAGED_ROOT_FILES:
        return True
    if any(relative == prefix or relative.startswith(prefix) for prefix in PERSISTENT_PREFIXES):
        return False
    if relative.startswith(MANAGED_CONFIG_PREFIXES):
        return True
    return relative.split("/", 1)[0] in MANAGED_ROOTS


def package_under_inbox(path: Path) -> bool:
    inbox = (USER_ROOT / "updates").resolve()
    try:
        path.resolve().relative_to(inbox)
        return True
    except ValueError:
        return False


def verify_signature(manifest_bytes: bytes, signature: bytes) -> None:
    if signature.startswith(b"UNSIGNED-DEVELOPMENT-PACKAGE"):
        if os.environ.get("PLUMOS_UPDATE_ALLOW_UNSIGNED") == "1":
            return
        raise UpdateError("unsigned development package is not allowed")
    if not PUBLIC_KEY.is_file():
        raise UpdateError(f"update public key is missing: {PUBLIC_KEY}")
    temp_root = Path(os.environ.get("PLUMOS_UPDATE_TMP", "/run"))
    if not temp_root.is_dir():
        temp_root = Path(tempfile.gettempdir())
    with tempfile.TemporaryDirectory(prefix="plumos-update-verify-", dir=temp_root) as temp:
        manifest_path = Path(temp) / "manifest.json"
        signature_path = Path(temp) / "manifest.sig"
        manifest_path.write_bytes(manifest_bytes)
        signature_path.write_bytes(signature)
        result = subprocess.run(
            [
                "openssl", "pkeyutl", "-verify", "-pubin", "-rawin",
                "-inkey", str(PUBLIC_KEY), "-sigfile", str(signature_path),
                "-in", str(manifest_path),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    if result.returncode != 0:
        raise UpdateError("package signature verification failed")


def load_package(path: Path) -> tuple[dict[str, Any], bytes, bytes]:
    if not path.is_file():
        raise UpdateError(f"update package is missing: {path}")
    try:
        with tarfile.open(path, "r:gz") as archive:
            members = archive.getmembers()
            names = {member.name for member in members}
            if len(names) != len(members):
                raise UpdateError("duplicate archive member is forbidden")
            if "META/manifest.json" not in names or "META/manifest.sig" not in names:
                raise UpdateError("package metadata is incomplete")
            for member in members:
                safe_relative(member.name)
                if not (member.name.startswith("META/") or member.name.startswith("payload/")):
                    raise UpdateError(f"undeclared package namespace: {member.name}")
                if member.ischr() or member.isblk() or member.isfifo() or member.isdev():
                    raise UpdateError(f"special archive member is forbidden: {member.name}")
                if member.islnk():
                    raise UpdateError(f"hardlinks are forbidden: {member.name}")
            manifest_handle = archive.extractfile("META/manifest.json")
            signature_handle = archive.extractfile("META/manifest.sig")
            if manifest_handle is None or signature_handle is None:
                raise UpdateError("cannot read package metadata")
            manifest_bytes = manifest_handle.read()
            signature = signature_handle.read()
    except (tarfile.TarError, OSError) as exc:
        raise UpdateError(f"cannot read update package: {exc}") from exc
    try:
        manifest = json.loads(manifest_bytes)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise UpdateError(f"invalid update manifest: {exc}") from exc
    verify_signature(manifest_bytes, signature)
    validate_manifest(manifest, names)
    return manifest, manifest_bytes, signature


def validate_manifest(manifest: dict[str, Any], archive_names: set[str]) -> None:
    if manifest.get("format") != 1:
        raise UpdateError("unsupported update package format")
    if manifest.get("package_type") not in {"runtime", "system"}:
        raise UpdateError("unsupported update package type")
    if manifest.get("device_id") != DEVICE_ID or manifest.get("architecture") != ARCHITECTURE:
        raise UpdateError("package target does not match Pixel2 aarch64")
    if manifest.get("vendor_runtime") != VENDOR_RUNTIME:
        raise UpdateError("vendor runtime mismatch")
    for key in ("version", "source_version", "payload_uncompressed_bytes"):
        if key not in manifest:
            raise UpdateError(f"manifest field is missing: {key}")
    files = manifest.get("files")
    if not isinstance(files, list):
        raise UpdateError("manifest files list is missing")
    declared: set[str] = set()
    declared_bytes = 0
    for entry in files:
        if not isinstance(entry, dict):
            raise UpdateError("invalid file manifest entry")
        relative = safe_relative(str(entry.get("path", "")))
        if relative in declared:
            raise UpdateError(f"duplicate payload path: {relative}")
        declared.add(relative)
        member = f"payload/{relative}"
        if member not in archive_names:
            raise UpdateError(f"payload member is missing: {relative}")
        if entry.get("type") not in {"file", "symlink"}:
            raise UpdateError(f"unsupported payload type: {relative}")
        if entry.get("type") == "symlink":
            target = PurePosixPath(str(entry.get("target", "")))
            if not str(target) or target.is_absolute() or ".." in target.parts:
                raise UpdateError(f"escaping symlink is forbidden: {relative}")
        size = int(entry.get("size", -1))
        if size < 0:
            raise UpdateError(f"invalid payload size: {relative}")
        declared_bytes += size
    unexpected = {
        name.removeprefix("payload/")
        for name in archive_names
        if name.startswith("payload/") and name.removeprefix("payload/") not in declared
    }
    if unexpected:
        raise UpdateError(f"undeclared payload member: {sorted(unexpected)[0]}")
    if declared_bytes != int(manifest.get("payload_uncompressed_bytes", -1)):
        raise UpdateError("declared payload byte count mismatch")
    if manifest["package_type"] == "runtime":
        for relative in declared:
            if not managed_runtime_path(relative):
                raise UpdateError(f"runtime package targets persistent path: {relative}")
        for relative in manifest.get("delete", []):
            relative = safe_relative(str(relative))
            if not managed_runtime_path(relative):
                raise UpdateError(f"runtime package deletes persistent path: {relative}")
    elif declared != {"system.squashfs"}:
        raise UpdateError("system package must contain only system.squashfs")


def current_compatibility(manifest: dict[str, Any]) -> None:
    current_vendor = read_text(PLUMOS_ROOT / "COMPAT_VENDOR", VENDOR_RUNTIME)
    if current_vendor != manifest["vendor_runtime"]:
        raise UpdateError("installed vendor runtime does not match package")
    if manifest["package_type"] == "runtime":
        current_version = read_text(PLUMOS_ROOT / "VERSION", "unknown")
    else:
        current_version = read_text(SYSTEM_VERSION_FILE, "unknown")
    source = str(manifest.get("source_version", "*"))
    if source not in {"*", current_version}:
        raise UpdateError(f"source version mismatch: installed={current_version} required={source}")
    if str(manifest.get("version")) == current_version:
        raise UpdateError(f"version is already installed: {current_version}")
    system_abi = read_text(SYSTEM_ABI_FILE)
    runtime_abi = read_text(RUNTIME_ABI_FILE)
    if not system_abi or not runtime_abi:
        raise UpdateError("installed ABI metadata is missing")
    if manifest["package_type"] == "runtime":
        compatible = str(manifest.get("system_abi", "")) == system_abi
        message = "system ABI does not match the runtime package"
    else:
        compatible = str(manifest.get("runtime_abi", "")) == runtime_abi
        message = "runtime ABI does not match the system package"
    if not compatible:
        raise UpdateError(message)


def inspect(path: Path) -> dict[str, Any]:
    manifest, _, _ = load_package(path)
    current_compatibility(manifest)
    return {
        "path": str(path),
        "sha256": sha256_file(path),
        "package_type": manifest["package_type"],
        "version": manifest["version"],
        "source_version": manifest.get("source_version", "*"),
        "payload_uncompressed_bytes": manifest["payload_uncompressed_bytes"],
    }


def scan_packages() -> list[dict[str, Any]]:
    inbox = USER_ROOT / "updates"
    results: list[dict[str, Any]] = []
    if not inbox.is_dir():
        return results
    for path in sorted(inbox.glob("plumos-pixel2-*.tar.gz")):
        try:
            item = inspect(path)
            item["mtime"] = path.stat().st_mtime_ns
            results.append(item)
        except UpdateError as exc:
            results.append({"path": str(path), "valid": False, "error": str(exc), "mtime": path.stat().st_mtime_ns})
    return results


def request(path: Path) -> dict[str, Any]:
    if os.environ.get("PLUMOS_UPDATE_ALLOW_EXTERNAL") != "1" and not package_under_inbox(path):
        raise UpdateError("package is outside the PLUMOS updates inbox")
    item = inspect(path)
    request_value = {**item, "requested_at": now()}
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    atomic_json(REQUEST_FILE, request_value)
    return request_value


def request_latest() -> dict[str, Any]:
    valid = [item for item in scan_packages() if item.get("package_type")]
    if not valid:
        raise UpdateError("no compatible update package found")
    latest = max(valid, key=lambda item: int(item.get("mtime", 0)))
    return request(Path(latest["path"]))


def extract_payload(package: Path, manifest: dict[str, Any], destination: Path) -> None:
    remove_path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    entries = {str(entry["path"]): entry for entry in manifest["files"]}
    with tarfile.open(package, "r:gz") as archive:
        for relative, entry in entries.items():
            member = archive.getmember(f"payload/{relative}")
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if entry["type"] == "symlink":
                if not member.issym() or member.linkname != entry.get("target"):
                    raise UpdateError(f"symlink metadata mismatch: {relative}")
                target.symlink_to(member.linkname)
                actual = hashlib.sha256(member.linkname.encode()).hexdigest()
            else:
                if not member.isfile() or member.size != int(entry["size"]):
                    raise UpdateError(f"file metadata mismatch: {relative}")
                source = archive.extractfile(member)
                if source is None:
                    raise UpdateError(f"cannot read payload: {relative}")
                with target.open("wb") as handle:
                    shutil.copyfileobj(source, handle, 1024 * 1024)
                    handle.flush()
                    fsync_file_descriptor(handle.fileno())
                os.chmod(target, int(entry["mode"]))
                actual = sha256_file(target)
            if actual != entry["sha256"]:
                raise UpdateError(f"staged payload hash mismatch: {relative}")
    fsync_directory(destination)


def journal_write(journal: dict[str, Any]) -> None:
    atomic_json(JOURNAL_FILE, journal)


def rollback_runtime(reason: str) -> None:
    show_progress("update_rollback")
    if not JOURNAL_FILE.is_file():
        RUNTIME_PENDING.unlink(missing_ok=True)
        return
    journal = json.loads(JOURNAL_FILE.read_text(encoding="utf-8"))
    for operation in reversed(journal.get("operations", [])):
        relative = safe_relative(operation["path"])
        target = PLUMOS_ROOT / relative
        backup = BACKUP_ROOT / "files" / relative
        if (operation.get("install_requested") or operation.get("installed")) and (
            target.exists() or target.is_symlink()
        ):
            remove_path(target)
        if operation.get("existed") and (backup.exists() or backup.is_symlink()):
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(backup, target)
        fsync_directory(target.parent)
    journal["status"] = "rolled_back"
    journal["rollback_reason"] = reason
    journal["rolled_back_at"] = now()
    journal_write(journal)
    RUNTIME_PENDING.unlink(missing_ok=True)
    atomic_json(LAST_RESULT, {"result": "rolled_back", "reason": reason, "time": now()})
    os.sync()


def runtime_sort_key(entry: dict[str, Any]) -> tuple[int, str]:
    return (1 if entry["path"] in METADATA_LAST else 0, str(entry["path"]))


def apply_runtime(package: Path, manifest: dict[str, Any]) -> int:
    show_progress("update_verify")
    staging = STAGING_ROOT / f"runtime-{manifest['version']}.partial"
    backup_bytes = 0
    for entry in manifest["files"]:
        target = PLUMOS_ROOT / entry["path"]
        if target.is_file() and not target.is_symlink():
            backup_bytes += target.stat().st_size
    for relative in manifest.get("delete", []):
        target = PLUMOS_ROOT / relative
        if target.is_file() and not target.is_symlink():
            backup_bytes += target.stat().st_size
    required = int(manifest["payload_uncompressed_bytes"]) + backup_bytes + 64 * 1024 * 1024
    if shutil.disk_usage(PLUMOS_ROOT).free < required:
        raise UpdateError(f"insufficient ext4 space: required={required}")
    extract_payload(package, manifest, staging)
    if RUNTIME_PENDING.is_file():
        rollback_runtime("pending runtime update did not reach frontend readiness")
    remove_path(BACKUP_ROOT)
    BACKUP_ROOT.mkdir(parents=True, exist_ok=True)
    journal: dict[str, Any] = {
        "status": "applying",
        "version": manifest["version"],
        "previous_version": read_text(PLUMOS_ROOT / "VERSION", "unknown"),
        "package_sha256": sha256_file(package),
        "started_at": now(),
        "operations": [],
    }
    journal_write(journal)
    operations: list[tuple[str, bool]] = [(str(entry["path"]), True) for entry in manifest["files"]]
    operations.extend((str(path), False) for path in manifest.get("delete", []))
    operations.sort(key=lambda item: runtime_sort_key({"path": item[0]}))
    try:
        show_progress("update_runtime")
        for relative, installs in operations:
            safe_relative(relative)
            target = PLUMOS_ROOT / relative
            backup = BACKUP_ROOT / "files" / relative
            staged = staging / relative
            existed = target.exists() or target.is_symlink()
            if existed and not (target.is_file() or target.is_symlink()):
                raise UpdateError(f"managed target is not a file or symlink: {relative}")
            operation = {
                "path": relative,
                "existed": existed,
                "install_requested": installs,
                "installed": False,
            }
            journal["operations"].append(operation)
            journal_write(journal)
            if existed:
                backup.parent.mkdir(parents=True, exist_ok=True)
                os.replace(target, backup)
                fsync_directory(backup.parent)
                operation["backed_up"] = True
                journal_write(journal)
            if installs:
                target.parent.mkdir(parents=True, exist_ok=True)
                os.replace(staged, target)
                if target.is_file() and not target.is_symlink():
                    with target.open("rb") as handle:
                        fsync_file_descriptor(handle.fileno())
                operation["installed"] = True
                fsync_directory(target.parent)
                journal_write(journal)
        # A Runtime update is one of the explicit integrity-check points. The
        # normal boot path trusts this completed generation and does not repeat
        # the full 1+ GiB hash pass before drawing the frontend.
        verify_runtime()
    except Exception as exc:
        rollback_runtime(f"transaction failed: {exc}")
        raise UpdateError(f"runtime transaction failed: {exc}") from exc
    journal["status"] = "pending_health"
    journal["applied_at"] = now()
    journal_write(journal)
    atomic_json(RUNTIME_PENDING, {"version": manifest["version"], "package_sha256": journal["package_sha256"]})
    REQUEST_FILE.unlink(missing_ok=True)
    remove_path(staging)
    show_progress("update_finalize")
    os.sync()
    return 0


def boot_mount_state() -> tuple[str, set[str]]:
    try:
        lines = Path("/proc/mounts").read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise UpdateError(f"cannot inspect PLUMOS_BOOT mount state: {exc}") from exc
    for line in lines:
        fields = line.split()
        if len(fields) >= 4 and fields[1] == str(BOOT_ROOT):
            return fields[0], set(fields[3].split(","))
    raise UpdateError(f"PLUMOS_BOOT is not mounted: {BOOT_ROOT}")


def remount_boot(mode: str) -> None:
    if os.environ.get("PLUMOS_UPDATE_BOOT_REMOUNT", "1") == "0":
        return
    source, _ = boot_mount_state()
    errors: list[str] = []
    for command in (
        ["mount", "-o", f"remount,{mode}", source, str(BOOT_ROOT)],
        ["mount", "-o", f"remount,{mode}", str(BOOT_ROOT)],
    ):
        result = subprocess.run(
            command,
            text=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=False,
        )
        _, options = boot_mount_state()
        if result.returncode == 0 and mode in options:
            return
        errors.append(result.stderr.strip() or f"rc={result.returncode}")
    raise UpdateError(f"cannot remount PLUMOS_BOOT {mode}: {'; '.join(errors)}")


def verify_boot_write() -> None:
    probe = BOOT_ROOT / "system-slots" / ".plumos-update-write-probe"
    try:
        with probe.open("xb") as handle:
            handle.write(b"plumOS update write probe\n")
            handle.flush()
            fsync_file_descriptor(handle.fileno())
        probe.unlink()
        fsync_directory(probe.parent)
    except OSError as exc:
        probe.unlink(missing_ok=True)
        raise UpdateError(f"PLUMOS_BOOT write probe failed: {exc}") from exc


def apply_system(package: Path, manifest: dict[str, Any], manifest_bytes: bytes, signature: bytes) -> int:
    show_progress("update_verify")
    staging = STAGING_ROOT / f"system-{manifest['version']}.partial"
    required = int(manifest["payload_uncompressed_bytes"]) + 64 * 1024 * 1024
    if shutil.disk_usage(PLUMOS_ROOT).free < required:
        raise UpdateError(f"insufficient ext4 staging space: required={required}")
    extract_payload(package, manifest, staging)
    source = staging / "system.squashfs"
    expected = str(manifest["files"][0]["sha256"])
    active = read_text(SYSTEM_ACTIVE, "a")
    if active not in {"a", "b"}:
        active = "a"
    inactive = "b" if active == "a" else "a"
    system_dir = BOOT_ROOT / "system-slots"
    temp_image = system_dir / f".system-{inactive}.squashfs.new"
    final_image = system_dir / f"system-{inactive}.squashfs"
    remounted_rw = False
    remount_error: UpdateError | None = None
    cleanup_error: UpdateError | None = None
    try:
        remount_boot("rw")
        remounted_rw = True
        verify_boot_write()
        show_progress("update_system")
        destination_fd: int | None = None
        read_only_states: list[str] = []
        for _ in range(3):
            # Keep the verified remount and destination open in the same retry
            # window. The vendor kernel may briefly restore boot-resource ro
            # after a metadata sync.
            remount_boot("rw")
            try:
                destination_fd = os.open(
                    temp_image, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644
                )
                break
            except OSError as exc:
                if exc.errno != errno.EROFS:
                    raise
                _, options = boot_mount_state()
                read_only_states.append(",".join(sorted(options)))
                time.sleep(0.1)
        if destination_fd is None:
            raise UpdateError(
                "PLUMOS_BOOT returned read-only while opening inactive slot: "
                + "; ".join(read_only_states)
            )
        with source.open("rb") as src, os.fdopen(destination_fd, "wb") as dst:
            shutil.copyfileobj(src, dst, 1024 * 1024)
            dst.flush()
            fsync_file_descriptor(dst.fileno())
        if sha256_file(temp_image) != expected:
            raise UpdateError("PLUMOS_BOOT readback hash mismatch")
        os.replace(temp_image, final_image)
        atomic_text(system_dir / f"system-{inactive}.sha256", f"{expected}  system-{inactive}.squashfs\n")
        atomic_bytes(system_dir / f"system-{inactive}.manifest.json", manifest_bytes)
        atomic_bytes(system_dir / f"system-{inactive}.manifest.sig", signature)
        os.sync()
    finally:
        operation_failed = sys.exc_info()[0] is not None
        try:
            temp_image.unlink(missing_ok=True)
        except OSError as exc:
            if not operation_failed:
                cleanup_error = UpdateError(
                    f"cannot remove inactive-slot temporary image: {exc}"
                )
        if remounted_rw:
            try:
                remount_boot("ro")
            except UpdateError as exc:
                remount_error = exc
    if cleanup_error is not None:
        raise cleanup_error
    if remount_error is not None:
        raise remount_error
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    atomic_text(SYSTEM_PENDING, f"{inactive}\n")
    SYSTEM_ATTEMPTED.unlink(missing_ok=True)
    atomic_json(
        STATE_ROOT / "system-pending.json",
        {"slot": inactive, "version": manifest["version"], "sha256": expected, "requested_at": now()},
    )
    REQUEST_FILE.unlink(missing_ok=True)
    remove_path(staging)
    atomic_json(LAST_RESULT, {"result": "system_staged", "slot": inactive, "version": manifest["version"], "time": now()})
    show_progress("update_finalize")
    os.sync()
    return 20


def recover_prior_runtime() -> None:
    if RUNTIME_PENDING.is_file():
        rollback_runtime("frontend readiness was not confirmed before reboot")
    elif JOURNAL_FILE.is_file():
        try:
            journal = json.loads(JOURNAL_FILE.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            raise UpdateError("runtime transaction journal is unreadable")
        if journal.get("status") == "applying":
            rollback_runtime("interrupted runtime transaction")


def apply_pending() -> int:
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    recover_prior_runtime()
    remove_path(STAGING_ROOT)
    STAGING_ROOT.mkdir(parents=True, exist_ok=True)
    if not REQUEST_FILE.is_file():
        return 0
    show_progress("update_verify")
    request_value = json.loads(REQUEST_FILE.read_text(encoding="utf-8"))
    package = Path(request_value["path"])
    if not package_under_inbox(package):
        raise UpdateError("pending package moved outside update inbox")
    if sha256_file(package) != request_value["sha256"]:
        raise UpdateError("pending package hash changed after request")
    manifest, manifest_bytes, signature = load_package(package)
    current_compatibility(manifest)
    if manifest["package_type"] != request_value["package_type"] or manifest["version"] != request_value["version"]:
        raise UpdateError("pending request metadata changed")
    if manifest["package_type"] == "runtime":
        return apply_runtime(package, manifest)
    return apply_system(package, manifest, manifest_bytes, signature)


def verify_runtime() -> None:
    checksum_file = PLUMOS_ROOT / "checksums.sha256"
    if not checksum_file.is_file():
        raise UpdateError("installed Runtime checksum manifest is missing")
    configured = os.environ.get("PLUMOS_UPDATE_SHA256SUM")
    if configured:
        command = [configured]
    elif Path("/bin/busybox").is_file():
        command = ["/bin/busybox", "sha256sum"]
    else:
        command = ["sha256sum"]
    result = subprocess.run(
        [*command, "-c", checksum_file.name],
        cwd=PLUMOS_ROOT,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip().splitlines()
        suffix = f": {detail[-1]}" if detail else ""
        raise UpdateError(f"installed Runtime checksum verification failed{suffix}")


def mark_healthy() -> None:
    changed = False
    if RUNTIME_PENDING.is_file():
        if not JOURNAL_FILE.is_file():
            raise UpdateError("runtime health state exists without transaction journal")
        pending = json.loads(RUNTIME_PENDING.read_text(encoding="utf-8"))
        journal = json.loads(JOURNAL_FILE.read_text(encoding="utf-8"))
        journal["status"] = "healthy"
        journal["healthy_at"] = now()
        journal_write(journal)
        RUNTIME_PENDING.unlink(missing_ok=True)
        atomic_json(LAST_RESULT, {"result": "runtime_healthy", "version": pending["version"], "time": now()})
        changed = True
    pending_slot = read_text(SYSTEM_PENDING)
    booted_slot = read_text(STATE_ROOT / "system-booted")
    active_slot = read_text(SYSTEM_ACTIVE)
    if pending_slot not in {"a", "b"} and active_slot not in {"a", "b"} and booted_slot in {"a", "b"}:
        atomic_text(SYSTEM_ACTIVE, f"{booted_slot}\n")
        atomic_json(LAST_RESULT, {"result": "system_baseline_healthy", "slot": booted_slot, "time": now()})
        changed = True
    if pending_slot in {"a", "b"}:
        if booted_slot != pending_slot:
            raise UpdateError(
                f"pending System slot was not booted: pending={pending_slot} "
                f"booted={booted_slot or 'unknown'}"
            )
        atomic_text(SYSTEM_ACTIVE, f"{pending_slot}\n")
        SYSTEM_PENDING.unlink(missing_ok=True)
        SYSTEM_ATTEMPTED.unlink(missing_ok=True)
        (STATE_ROOT / "system-pending.json").unlink(missing_ok=True)
        atomic_json(LAST_RESULT, {"result": "system_healthy", "slot": pending_slot, "time": now()})
        changed = True
    if changed:
        os.sync()


def print_item(item: dict[str, Any]) -> None:
    for key in ("path", "package_type", "version", "source_version", "sha256", "payload_uncompressed_bytes"):
        if key in item:
            print(f"{key}={item[key]}")


def record_failure(message: str) -> None:
    value = {"result": "failed", "error": message, "time": now()}
    try:
        STATE_ROOT.mkdir(parents=True, exist_ok=True)
        atomic_json(LAST_RESULT, value)
    except OSError:
        pass
    try:
        atomic_json(USER_ROOT / "plumos-logs/update/last-failure.json", value)
    except OSError:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("package", type=Path)
    request_parser = subparsers.add_parser("request")
    request_parser.add_argument("package", type=Path)
    subparsers.add_parser("scan")
    subparsers.add_parser("request-latest")
    subparsers.add_parser("apply-pending")
    subparsers.add_parser("verify-runtime")
    subparsers.add_parser("mark-healthy")
    args = parser.parse_args()
    try:
        if args.command == "inspect":
            print_item(inspect(args.package))
        elif args.command == "scan":
            for item in scan_packages():
                print(json.dumps(item, sort_keys=True))
        elif args.command == "request":
            with update_lock():
                item = request(args.package)
            print_item(item)
            print("result=ready")
        elif args.command == "request-latest":
            with update_lock():
                item = request_latest()
            print_item(item)
            print("result=ready")
        elif args.command == "apply-pending":
            with update_lock():
                return apply_pending()
        elif args.command == "verify-runtime":
            with update_lock():
                verify_runtime()
            print("runtime_verify=result-ok")
        elif args.command == "mark-healthy":
            with update_lock():
                mark_healthy()
        return 0
    except UpdateError as exc:
        record_failure(str(exc))
        show_progress("update_error")
        print(f"error={exc}", file=sys.stderr)
        return 1
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        message = f"update operation failed: {exc}"
        record_failure(message)
        show_progress("update_error")
        print(f"error={message}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
