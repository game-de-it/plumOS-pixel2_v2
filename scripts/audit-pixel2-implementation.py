#!/usr/bin/env python3
"""Audit the Pixel2 product surface against the generated app-layer.

The normal report is informational and may be used while the port is still in
development.  --release-gate fails while a user-visible route or setting is
advertised without its managed Pixel2 implementation.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any


LANGUAGE_FILES = ("en.lang", "ja.lang", "ch.lang", "pt.lang", "fr.lang", "de.lang")
REQUIRED_COMPONENTS = (
    "frontend",
    "retroarch",
    "libretro-cores",
    "picoarch",
    "standalone",
    "audio-router",
    "pyxel",
)
SHARED_APPS = {
    "scraping": "Scraping and thumbnail fetch",
    "file_manager": "File Manager",
    "music_player": "Music Player",
    "retroarch": "RetroArch menu",
    "pyxel_setup": "Pyxel Setup",
    "portmaster": "PortMaster",
    "portmaster_update": "Update PortMaster",
}
USER_SURFACE_HELPERS = {
    "plumos-time-sync": "System Settings / Time Settings",
    "plumos-storage-health": "System Settings / Storage Check",
    "plumos-factory-reset": "System Settings / Factory Reset",
    "plumos-thumbnail-scraper": "Apps / Scraping and Gallery fetch",
    "plumos-sdcard-cleanup": "post-scan removable-storage cleanup",
}


@dataclass(frozen=True)
class Finding:
    severity: str
    category: str
    item: str
    evidence: str
    release_blocker: bool = False


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def git_ref(repo: Path) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(repo), "rev-parse", "--short", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def shell_helper(profile: str) -> str | None:
    match = re.search(r"(?:^|/)bin/(plumos-[A-Za-z0-9_-]+)", profile)
    return match.group(1) if match else None


def audit(repo: Path, app_root: Path) -> dict[str, Any]:
    systems = load_json(repo / "package/frontend-pixel2/systems.json")["systems"]
    apps = load_json(repo / "package/frontend-pixel2/apps.json")["apps"]
    findings: list[Finding] = []

    enabled = [system for system in systems if system.get("enabled") is not False]
    disabled = [system for system in systems if system.get("enabled") is False]
    policy_pending = [
        system
        for system in enabled
        if str(system.get("scraper", {}).get("reason", "")).endswith("pending")
    ]

    if not app_root.is_dir():
        findings.append(
            Finding(
                "P0",
                "build",
                "generated app-layer",
                f"missing: {app_root}",
                True,
            )
        )
        standalone_status: dict[str, str] = {}
        component_names: set[str] = set()
    else:
        component_names = {
            path.parent.name
            for path in (app_root / "components").glob("*/manifest.json")
        }
        for component in REQUIRED_COMPONENTS:
            if component not in component_names:
                findings.append(
                    Finding(
                        "P0",
                        "build",
                        f"component:{component}",
                        "component manifest missing from generated app-layer",
                        True,
                    )
                )
        standalone_manifest = app_root / "components/standalone/manifest.json"
        if standalone_manifest.is_file():
            standalone_status = {
                entry["id"]: entry.get("status", "unknown")
                for entry in load_json(standalone_manifest).get("emulators", [])
            }
        else:
            standalone_status = {}

    for system in enabled:
        for profile in system.get("launch_profiles", []):
            if not profile.startswith("standalone:"):
                continue
            emulator = profile.split(":", 1)[1]
            status = standalone_status.get(emulator, "missing")
            if status != "built":
                findings.append(
                    Finding(
                        "P0",
                        "frontend-route",
                        f"{system['id']} -> {profile}",
                        f"standalone manifest status={status}",
                        True,
                    )
                )

    visible_app_ids = {app["id"] for app in apps if app.get("visible", True)}
    for app in apps:
        if not app.get("visible", True):
            continue
        helper = shell_helper(str(app.get("launch_profile", "")))
        if helper and not (app_root / "bin" / helper).is_file():
            findings.append(
                Finding(
                    "P0",
                    "apps",
                    app["id"],
                    f"visible entry requires missing bin/{helper}",
                    True,
                )
            )
    for app_id, label in SHARED_APPS.items():
        if app_id not in visible_app_ids:
            findings.append(
                Finding(
                    "P1",
                    "apps-parity",
                    app_id,
                    f"{label} is present in the shared handheld surface but not implemented on Pixel2",
                )
            )

    for helper, surface in USER_SURFACE_HELPERS.items():
        if not (app_root / "bin" / helper).is_file():
            findings.append(
                Finding(
                    "P0",
                    "frontend-helper",
                    helper,
                    f"missing backend for {surface}",
                    True,
                )
            )

    controller_source = repo / "vendor/plumos-frontend/src/plumos_controller_ui.c"
    controller_text = controller_source.read_text(encoding="utf-8", errors="replace")
    network_services = app_root / "bin/plumos-network-services"
    network_text = (
        network_services.read_text(encoding="utf-8", errors="replace")
        if network_services.is_file()
        else ""
    )
    if "Manual update: overwrite plumOS files on the SD card" in controller_text:
        findings.append(
            Finding(
                "P0",
                "frontend-placeholder",
                "System Update",
                "the selectable action only reports a manual overwrite instruction",
                True,
            )
        )
    if (
        "Pixel2 has no lid switch." in controller_text
        and 'if (!runtime_device_is_pixel2()) {\n    add_bool_setting_entry(ui, "system_lid_suspend"'
        not in controller_text
    ):
        findings.append(
            Finding(
                "P0",
                "frontend-placeholder",
                "Lid Suspend",
                "Pixel2 exposes a selectable lid setting even though the device has no lid switch",
                True,
            )
        )
    volume_control = app_root / "bin/plumos-volume-control"
    volume_text = (
        volume_control.read_text(encoding="utf-8", errors="replace")
        if volume_control.is_file()
        else ""
    )
    if (
        "pixel2-state-only" in volume_text
        and 'if (!runtime_device_is_pixel2()) {\n    add_setting_entry(ui, "system_audio_output"'
        not in controller_text
    ):
        findings.append(
            Finding(
                "P0",
                "frontend-placeholder",
                "Audio Output",
                "Speaker/Headphone is selectable but the Pixel2 helper only stores logical state",
                True,
            )
        )
    pixel2_absent_services_hidden = (
        'if (!runtime_device_is_pixel2()) {\n    add_bool_setting_entry(ui, "network_ftp_enabled"'
        in controller_text
    )
    for service in ("ftp", "sftp", "samba"):
        if re.search(rf"{service}\).*not_installed|{service}[^\n]*not packaged", network_text):
            if pixel2_absent_services_hidden:
                continue
            findings.append(
                Finding(
                    "P0",
                    "network-service",
                    service,
                    "service is selectable in Network Settings but explicitly not packaged",
                    True,
                )
            )
    adbd_service = repo / "rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
    adbd_text = (
        adbd_service.read_text(encoding="utf-8", errors="replace")
        if adbd_service.is_file()
        else ""
    )
    adb_has_explicit_opt_in = (
        "adb_opted_in()" in adbd_text
        and "explicit-opt-in-required" in adbd_text
        and "/mnt/plumos-user/plumos-enable-adb" in adbd_text
    )
    if adbd_service.is_file() and not adb_has_explicit_opt_in:
        findings.append(
            Finding(
                "P0",
                "connectivity-security",
                "ADB authentication",
                "release image still boots development adbd with authentication disabled",
                True,
            )
        )

    lang_root = app_root / "share/frontend/lang"
    for language in LANGUAGE_FILES:
        if not (lang_root / language).is_file():
            findings.append(
                Finding(
                    "P0",
                    "frontend-language",
                    language,
                    "language is selectable by the frontend but the translation file is absent",
                    True,
                )
            )

    logo_root = app_root / "themes/default/logos/systems"
    for system in enabled:
        if not (logo_root / f"{system['id']}.png").is_file():
            findings.append(
                Finding(
                    "P1",
                    "frontend-theme",
                    system["id"],
                    "enabled system has no default theme logo",
                    True,
                )
            )

    for emulator, status in sorted(standalone_status.items()):
        if status != "built":
            findings.append(
                Finding(
                    "P1",
                    "standalone-backlog",
                    emulator,
                    f"standalone manifest status={status}",
                )
            )
    for system in disabled:
        findings.append(
            Finding(
                "P2",
                "disabled-system",
                system["id"],
                str(system.get("scraper", {}).get("reason", "no reason recorded")),
            )
        )
    for system in policy_pending:
        findings.append(
            Finding(
                "P1",
                "content-policy",
                system["id"],
                str(system.get("scraper", {}).get("reason", "policy pending")),
            )
        )

    findings.sort(key=lambda item: (item.severity, item.category, item.item))
    blockers = [finding for finding in findings if finding.release_blocker]
    return {
        "device": "pixel2",
        "source_ref": git_ref(repo),
        "metrics": {
            "systems_total": len(systems),
            "systems_enabled": len(enabled),
            "systems_disabled": len(disabled),
            "enabled_policy_pending": len(policy_pending),
            "visible_apps": len(visible_app_ids),
            "required_components_present": len(component_names & set(REQUIRED_COMPONENTS)),
            "required_components_total": len(REQUIRED_COMPONENTS),
            "standalone_built": sum(status == "built" for status in standalone_status.values()),
            "standalone_pending": sum(status != "built" for status in standalone_status.values()),
            "release_blockers": len(blockers),
            "findings": len(findings),
        },
        "findings": [asdict(finding) for finding in findings],
    }


def markdown(report: dict[str, Any], app_root: Path) -> str:
    metrics = report["metrics"]
    lines = [
        "# Pixel2 implementation audit",
        "",
        f"Source ref: `{report['source_ref']}`",
        f"App root: `{app_root}`",
        "",
        "## Summary",
        "",
        f"- systems: {metrics['systems_enabled']} enabled / {metrics['systems_total']} total",
        f"- enabled systems with pending content policy: {metrics['enabled_policy_pending']}",
        f"- visible Apps entries: {metrics['visible_apps']}",
        f"- required components: {metrics['required_components_present']}/{metrics['required_components_total']}",
        f"- standalone: {metrics['standalone_built']} built / {metrics['standalone_pending']} pending",
        f"- release blockers: {metrics['release_blockers']}",
        "",
        "## Findings",
        "",
        "| priority | category | item | evidence | release blocker |",
        "| --- | --- | --- | --- | --- |",
    ]
    for finding in report["findings"]:
        evidence = str(finding["evidence"]).replace("|", "\\|")
        lines.append(
            f"| {finding['severity']} | {finding['category']} | `{finding['item']}` | "
            f"{evidence} | {'yes' if finding['release_blocker'] else 'no'} |"
        )
    if not report["findings"]:
        lines.append("| - | - | - | no findings | no |")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    parser.add_argument("--app-root", default="output/app-layer/pixel2/plumos")
    parser.add_argument("--json", default="")
    parser.add_argument("--markdown", default="")
    parser.add_argument("--release-gate", action="store_true")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    app_root = Path(args.app_root)
    if not app_root.is_absolute():
        app_root = repo / app_root
    report = audit(repo, app_root)
    rendered = markdown(report, app_root)

    if args.json:
        path = Path(args.json)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.markdown:
        path = Path(args.markdown)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(rendered, encoding="utf-8")
    if not args.json and not args.markdown:
        print(rendered, end="")

    blockers = report["metrics"]["release_blockers"]
    if args.release_gate and blockers:
        print(f"implementation_audit=result-failed release_blockers={blockers}")
        return 1
    print(f"implementation_audit=result-ok release_blockers={blockers}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
