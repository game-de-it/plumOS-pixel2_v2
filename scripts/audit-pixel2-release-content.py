#!/usr/bin/env python3
"""Reject private/user content and stale release identity from Pixel2 outputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
USER_CONTENT_SUFFIXES = {
    ".3ds", ".7z", ".cso", ".dci", ".fds", ".gb", ".gba", ".gbc",
    ".gg", ".nds", ".nes", ".p8", ".pbp", ".rom", ".sav",
    ".sfc", ".smc", ".sms", ".state", ".vms", ".zip",
}
PROHIBITED_NAMES = {
    "authorized_keys", "id_dsa", "id_ecdsa", "id_ed25519", "id_rsa",
    "wpa_supplicant.conf",
}
MUTABLE_TOP_LEVEL = {"logs", "saves", "states", "state", "updates"}
PRIVATE_KEY_RE = re.compile(
    br"-----BEGIN (?:RSA |OPENSSH |EC |DSA )?PRIVATE KEY-----"
)


def contains_private_key_text(data: bytes) -> bool:
    """Match PEM credentials, but not parser literals embedded in ELF files."""
    return b"\0" not in data[:8192] and PRIVATE_KEY_RE.search(data) is not None


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_output(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(ROOT), *args], text=True
    ).strip()


def tracked_content_findings() -> list[str]:
    findings: list[str] = []
    for raw in git_output("ls-files", "-z").split("\0"):
        if not raw:
            continue
        path = Path(raw)
        lower_parts = {part.casefold() for part in path.parts}
        if lower_parts & {"rom", "roms", "bios"} and path.suffix.casefold() in USER_CONTENT_SUFFIXES:
            findings.append(f"tracked user content: {path}")
        if path.name.casefold() in PROHIBITED_NAMES:
            findings.append(f"tracked credential file: {path}")
        full = ROOT / path
        if full.is_file() and full.stat().st_size <= 4 * 1024 * 1024:
            try:
                data = full.read_bytes()
            except OSError:
                continue
            if contains_private_key_text(data):
                findings.append(f"tracked private key material: {path}")
    return findings


def app_findings(app_root: Path) -> list[str]:
    findings: list[str] = []
    if not app_root.is_dir():
        return [f"app layer missing: {app_root}"]
    for path in app_root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(app_root)
        parts = tuple(part.casefold() for part in rel.parts)
        name = path.name.casefold()
        suffix = path.suffix.casefold()
        if parts and parts[0] in MUTABLE_TOP_LEVEL:
            findings.append(f"mutable app-layer file: {rel}")
        if name in PROHIBITED_NAMES:
            findings.append(f"credential file in app layer: {rel}")
        if ("roms" in parts or "bios" in parts) and suffix in USER_CONTENT_SUFFIXES:
            findings.append(f"ROM/BIOS content in app layer: {rel}")
        if name in {"pico8", "pico8_64", "pico8_dyn", "pico8.dat"}:
            findings.append(f"proprietary PICO-8 runtime in app layer: {rel}")
        if path.stat().st_size <= 4 * 1024 * 1024:
            try:
                data = path.read_bytes()
            except OSError:
                continue
            if contains_private_key_text(data):
                findings.append(f"private key material in app layer: {rel}")

    manifest = app_root / "manifest.json"
    checksums = app_root / "checksums.sha256"
    if not manifest.is_file() or not checksums.is_file():
        findings.append("app-layer manifest/checksums missing")
    else:
        payload = json.loads(manifest.read_text(encoding="utf-8"))
        if payload.get("device") != "pixel2" or payload.get("complete") is not True:
            findings.append("app-layer identity is not complete Pixel2")
    return findings


def image_findings(image: Path, image_manifest: Path) -> list[str]:
    findings: list[str] = []
    if not image.is_file():
        return [f"release image missing: {image}"]
    if not image_manifest.is_file():
        return [f"image manifest missing: {image_manifest}"]
    values: dict[str, str] = {}
    for line in image_manifest.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    if values.get("file") != image.name:
        findings.append("image manifest filename mismatch")
    if values.get("image_size") != str(image.stat().st_size):
        findings.append("image manifest size mismatch")
    if values.get("image_sha256") != sha256(image):
        findings.append("image manifest SHA-256 mismatch")
    if values.get("user_filesystem") != "created-on-first-boot":
        findings.append("release seed unexpectedly contains a user filesystem")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--app-root", type=Path,
        default=ROOT / "output/app-layer/pixel2/plumos",
    )
    parser.add_argument("--image", type=Path)
    parser.add_argument("--image-manifest", type=Path)
    parser.add_argument("--tracked-only", action="store_true")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    findings = tracked_content_findings()
    if not args.tracked_only:
        findings.extend(app_findings(args.app_root.resolve()))
        if args.image:
            manifest = args.image_manifest or args.image.with_name("image.manifest")
            findings.extend(image_findings(args.image.resolve(), manifest.resolve()))

    report = {
        "device": "pixel2",
        "source_ref": git_output("rev-parse", "--short", "HEAD"),
        "tracked_only": args.tracked_only,
        "findings": findings,
        "result": "failed" if findings else "ok",
    }
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    if findings:
        for finding in findings:
            print(f"release-content: FAIL: {finding}")
        print(f"release_content=result-failed findings={len(findings)}")
        return 1
    print("release_content=result-ok findings=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
