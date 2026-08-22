#!/usr/bin/env python3
"""Exercise Pixel2 launch routes and collect machine-readable display/audio proof.

The validator is intentionally conservative.  It can prove that an emulator
stays alive, that at least one DRM plane contains changing/non-black pixels,
and that the ALSA playback pointer advances.  It cannot prove that controls,
orientation, aspect ratio, audible quality, or game play are correct; those
remain explicit manual checks in the generated report.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time
from typing import Any

from PIL import Image, ImageChops, ImageStat


REMOTE_INVENTORY = r"""
import json, os
with open('/mnt/plumos/config/frontend/systems.json', encoding='utf-8') as f:
    catalog = json.load(f)
rows = []
for system in catalog.get('systems', []):
    if system.get('enabled') is False:
        continue
    system_id = system['id']
    cache = '/mnt/plumos/state/frontend/systems/' + system_id + '.json'
    roms = []
    error = None
    if os.path.exists(cache):
        try:
            with open(cache, encoding='utf-8') as f:
                cached = json.load(f)
            for cached_system in cached.get('systems', []):
                for rom in cached_system.get('roms', []):
                    relative = rom.get('relative_path')
                    if relative:
                        roms.append(relative)
        except Exception as exc:
            error = str(exc)
    rows.append({
        'id': system_id,
        'name': system.get('display_name', system_id),
        'default_profile': system.get('default_launch_profile') or '',
        'profiles': system.get('launch_profiles', []),
        'roms': roms,
        'rom_error': error,
    })
print(json.dumps(rows, ensure_ascii=False))
"""


def command_prefix(password: str) -> list[str]:
    return ["sshpass", "-p", password] if password else []


class Device:
    def __init__(self, host: str, user: str, password: str) -> None:
        self.destination = f"{user}@{host}"
        self.password = password
        self.ssh_base = command_prefix(password) + [
            "ssh",
            "-o",
            "StrictHostKeyChecking=no",
            "-o",
            "ConnectTimeout=8",
            self.destination,
        ]
        self.scp_base = command_prefix(password) + [
            "scp",
            "-q",
            "-o",
            "StrictHostKeyChecking=no",
            "-o",
            "ConnectTimeout=8",
        ]

    def run(
        self,
        script: str,
        *,
        input_text: str | None = None,
        check: bool = True,
        timeout: int = 30,
    ) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            self.ssh_base + [script],
            input=input_text,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        if check and result.returncode:
            raise RuntimeError(
                f"remote command failed ({result.returncode}): {script}\n"
                f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
            )
        return result

    def copy_to(self, local: Path, remote: str) -> None:
        subprocess.run(
            self.scp_base + [str(local), f"{self.destination}:{remote}"],
            check=True,
            timeout=30,
        )

    def copy_from(self, remote: str, local: Path) -> None:
        subprocess.run(
            self.scp_base + [f"{self.destination}:{remote}", str(local)],
            check=True,
            timeout=30,
        )


def select_rom(roms: list[str]) -> str | None:
    """Avoid scanner false positives from SAVE/STATE directories."""
    rejected_parts = {"save", "saves", "state", "states", "savestate", "savestates"}
    for rom in roms:
        parts = {part.casefold() for part in Path(rom).parts[:-1]}
        if not parts.intersection(rejected_parts):
            return rom
    return roms[0] if roms else None


def parse_audio(text: str) -> dict[str, Any]:
    out: dict[str, Any] = {"raw": text.rstrip()}
    for line in text.splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip()
        if key in {"hw_ptr", "appl_ptr", "owner_pid"}:
            try:
                out[key] = int(value)
            except ValueError:
                out[key] = value
        elif key == "state":
            out[key] = value
    return out


PLANE_RE = re.compile(
    r"plane=(?P<plane>\d+).*?width=(?P<width>\d+) height=(?P<height>\d+) "
    r"format=(?P<format>\S+) pitch=(?P<pitch>\d+).*?crtc_xy=(?P<x>-?\d+),(?P<y>-?\d+)"
)


def decode_plane(
    raw_path: Path, width: int, height: int, pitch: int, pixel_format: str
) -> Image.Image | None:
    raw = raw_path.read_bytes()
    if pixel_format in {"XR24", "AR24"}:
        return Image.frombytes("RGB", (width, height), raw, "raw", "BGRX", pitch, 1)
    if pixel_format == "RG16":
        return Image.frombytes("RGB", (width, height), raw, "raw", "BGR;16", pitch, 1)
    return None


def image_metrics(image: Image.Image) -> dict[str, Any]:
    sample = image.copy()
    sample.thumbnail((160, 160))
    stat = ImageStat.Stat(sample)
    red, green, blue = sample.split()
    max_channel = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    histogram = max_channel.histogram()
    pixel_count = sample.width * sample.height
    nonblack = pixel_count - sum(histogram[:5])
    colors = sample.getcolors(maxcolors=160 * 160)
    return {
        "size": list(image.size),
        "mean": [round(value, 3) for value in stat.mean],
        "stddev": [round(value, 3) for value in stat.stddev],
        "nonblack_ratio": round(nonblack / max(1, pixel_count), 6),
        "sample_unique_colors": len(colors) if colors is not None else 160 * 160,
        "bbox": list(image.getbbox()) if image.getbbox() else None,
    }


def analyze_capture(directory: Path, base: str) -> dict[str, Any]:
    metadata_path = directory / f"{base}.capture.log"
    metadata = metadata_path.read_text(encoding="utf-8", errors="replace")
    planes = []
    for match in PLANE_RE.finditer(metadata):
        info: dict[str, Any] = {
            key: (int(value) if key not in {"format"} else value)
            for key, value in match.groupdict().items()
        }
        raw_path = directory / f"{base}-plane-{info['plane']}.xrgb8888"
        if not raw_path.exists():
            info["decode_error"] = "raw plane missing"
            planes.append(info)
            continue
        image = decode_plane(
            raw_path,
            info["width"],
            info["height"],
            info["pitch"],
            info["format"],
        )
        if image is None:
            info["decode_error"] = f"unsupported format {info['format']}"
            planes.append(info)
            continue
        physical = directory / f"{base}-plane-{info['plane']}-physical.png"
        logical = directory / f"{base}-plane-{info['plane']}-logical-cw.png"
        image.save(physical)
        image.rotate(-90, expand=True).save(logical)
        info["physical_png"] = physical.name
        info["logical_png"] = logical.name
        info["metrics"] = image_metrics(image)
        planes.append(info)
    # The Pixel2 cursor plane is normally 64x64.  It must never turn a failed
    # emulator launch into a display pass, so require a panel-sized plane.
    visible = [
        plane
        for plane in planes
        if plane.get("width", 0) >= 400
        and plane.get("height", 0) >= 400
        if plane.get("metrics", {}).get("nonblack_ratio", 0) >= 0.01
        and max(plane.get("metrics", {}).get("stddev", [0])) >= 2
    ]
    return {
        "metadata": metadata.rstrip(),
        "planes": planes,
        "visible_plane_ids": [plane["plane"] for plane in visible],
        "machine_visible": bool(visible),
    }


def safe_slug(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_.-]+", "-", value).strip("-")[:80]


def stop_launch(device: Device, pid: int, profile: str) -> None:
    """Stop the emulator first so its launcher can reap it and clean state."""
    if not pid:
        return
    if profile.startswith("picoarch:"):
        graceful = (
            "PLUMOS_ROOT=/mnt/plumos PLUMOS_RUNTIME_ROOT=/run/plumos "
            "/mnt/plumos/bin/plumos-picoarch-stop"
        )
    elif profile.startswith("standalone:"):
        emulator_id = shlex.quote(profile.split(":", 1)[1])
        graceful = (
            "PLUMOS_ROOT=/mnt/plumos PLUMOS_RUNTIME_ROOT=/run/plumos "
            "PLUMOS_USER_ROOT=/mnt/plumos-user "
            f"/mnt/plumos/bin/plumos-standalone-stop {emulator_id}"
        )
    else:
        # RetroArch and Pyxel have no external stop helper.  Signal only the
        # real emulator children first.  Killing the whole process group at
        # once prevents the shell/text-UI launcher chain from wait(2)-reaping
        # its children and leaves stale runtime state on the minimal system.
        graceful = (
            f"ps -eo pid,pgid,stat,comm | awk -v pgid={pid} "
            "'$2 == pgid && $3 !~ /^Z/ && "
            "$4 != \"plumos-text-ui\" && $4 != \"sh\" && $4 != \"bash\" "
            "{print $1}' | while read child; do "
            "kill -TERM \"$child\" 2>/dev/null || true; done"
        )
    device.run(
        f"{graceful} >/dev/null 2>&1 || true; "
        "tries=0; while [ $tries -lt 50 ]; do "
        f"live=$(ps -eo pgid,stat | awk -v pgid={pid} "
        "'$1 == pgid && $2 !~ /^Z/ {n++} END {print n+0}'); "
        "[ \"$live\" -eq 0 ] && break; sleep 0.1; tries=$((tries + 1)); done; "
        # A launcher which did not unwind after its emulator received TERM is
        # stopped as a final bounded fallback.  KILL is reserved for processes
        # that remain after another grace period.
        f"kill -TERM -{pid} 2>/dev/null || true; sleep 1; "
        f"kill -KILL -{pid} 2>/dev/null || true",
        check=False,
        timeout=12,
    )


def launch_one(
    device: Device,
    output_dir: Path,
    system: str,
    profile: str,
    rom: str,
    seconds: int,
) -> dict[str, Any]:
    slug = safe_slug(f"{system}-{profile.replace(':', '-')}")
    base = f"plumos-media-smoke-{slug}"
    prefix = f"/tmp/{base}"
    qsystem, qprofile, qrom = map(shlex.quote, (system, profile, rom))
    device.run(
        "/mnt/plumos/bin/plumos-frontend-stop stop >/dev/null 2>&1 || true; "
        f"setsid /bin/sh -c {shlex.quote('exec env PLUMOS_ROOT=/mnt/plumos PLUMOS_SDCARD_ROOT=/mnt/plumos-user PLUMOS_RUNTIME_ROOT=/run/plumos /mnt/plumos/bin/plumos-text-ui launch ' + qsystem + ' ' + qrom + ' --profile ' + qprofile + ' --execute --no-scan')} "
        f">{prefix}.launch.log 2>&1 </dev/null & echo $! >{prefix}.pid"
    )
    time.sleep(seconds)
    probe = device.run(
        f"pid=$(cat {prefix}.pid 2>/dev/null || true); "
        f"ps -eo pid,ppid,pgid,stat,comm,args >{prefix}.ps; "
        f"/tmp/drm-scanout-capture /dev/dri/card0 {prefix} 2>{prefix}.capture.log || true; "
        f"cat /proc/asound/card0/pcm0p/sub0/status >{prefix}.audio1 2>&1 || true; "
        "sleep 1; "
        f"cat /proc/asound/card0/pcm0p/sub0/status >{prefix}.audio2 2>&1 || true; "
        "cat /sys/class/power_supply/battery/capacity 2>/dev/null || true",
        timeout=20,
    )
    battery = probe.stdout.strip().splitlines()[-1] if probe.stdout.strip() else ""
    remote_files = device.run(
        f"for f in {prefix}*; do [ -f \"$f\" ] && printf '%s\\n' \"$f\"; done",
        check=False,
    ).stdout.splitlines()
    for remote_file in remote_files:
        device.copy_from(remote_file, output_dir / Path(remote_file).name)

    pid_text = (output_dir / f"{base}.pid").read_text().strip()
    pid = int(pid_text) if pid_text.isdigit() else 0
    ps_text = (output_dir / f"{base}.ps").read_text(
        encoding="utf-8", errors="replace"
    )
    live_processes = []
    for line in ps_text.splitlines():
        columns = line.split(None, 5)
        if len(columns) < 5 or not pid or columns[2] != str(pid):
            continue
        stat, command = columns[3], columns[4]
        if stat.startswith("Z") or command in {"plumos-text-ui", "sh", "bash"}:
            continue
        live_processes.append(line.strip())
    launch_log = (output_dir / f"{base}.launch.log").read_text(
        encoding="utf-8", errors="replace"
    )
    capture = analyze_capture(output_dir, base)
    audio1 = parse_audio(
        (output_dir / f"{base}.audio1").read_text(encoding="utf-8", errors="replace")
    )
    audio2 = parse_audio(
        (output_dir / f"{base}.audio2").read_text(encoding="utf-8", errors="replace")
    )
    audio_advancing = (
        audio1.get("state") == "RUNNING"
        and audio2.get("state") == "RUNNING"
        and isinstance(audio1.get("hw_ptr"), int)
        and isinstance(audio2.get("hw_ptr"), int)
        and audio2["hw_ptr"] != audio1["hw_ptr"]
    )
    launch_failed = "execute: failed" in launch_log or "launch command failed" in launch_log
    startup = "pass" if live_processes and not launch_failed else "fail"
    if capture["machine_visible"]:
        screen = "pass"
    elif startup == "fail":
        screen = "fail"
    else:
        screen = "manual"
    audio = "pass" if audio_advancing else ("fail" if startup == "fail" else "manual")
    if startup == "fail" or screen == "fail":
        overall = "fail"
    elif screen == "manual" or audio == "manual":
        overall = "manual"
    else:
        overall = "pass"

    stop_launch(device, pid, profile)
    return {
        "system": system,
        "profile": profile,
        "rom": rom,
        "startup": startup,
        "screen": screen,
        "audio": audio,
        "overall": overall,
        "live_process": live_processes[0] if live_processes else "",
        "live_processes": live_processes,
        "launch_log": launch_log.rstrip(),
        "capture": capture,
        "audio_samples": [audio1, audio2],
        "battery_percent_after": battery,
        "manual_checks": [
            "画面の向き、アスペクト比、欠け、色、ちらつき",
            "実際に聞こえる音、音質、音飛び、映像との同期",
            "入力、メニュー、終了、セーブ/ロード",
        ],
    }


def write_markdown(path: Path, report: dict[str, Any]) -> None:
    lines = [
        "# Pixel2 device startup/display/audio smoke",
        "",
        f"- generated: `{report['generated']}`",
        f"- host: `{report['device']}`",
        f"- tested routes: `{len(report['results'])}`",
        f"- enabled systems without cached ROM: `{len(report['untested_systems'])}`",
        "",
        "`screen=pass` はDRM planeに非黒・非単色の画像が存在すること、"
        "`audio=pass` はALSA playback pointerが進むことだけを表します。"
        "向き/比率/色/ちらつき、可聴音の品質、操作は目視・実機確認が必要です。",
        "",
        "| system | profile | startup | screen | audio | overall | battery |",
        "|---|---|---:|---:|---:|---:|---:|",
    ]
    for row in report["results"]:
        lines.append(
            f"| `{row['system']}` | `{row['profile']}` | {row['startup']} | "
            f"{row['screen']} | {row['audio']} | **{row['overall']}** | "
            f"{row['battery_percent_after']}% |"
        )
    lines.extend(["", "## Manual review queue", ""])
    manual = [row for row in report["results"] if row["overall"] != "pass"]
    if not manual:
        lines.append("- Machine checks did not leave an ambiguous route.")
    for row in manual:
        reasons = []
        if row["startup"] != "pass":
            reasons.append("launcher/emulator process not alive")
        if row["screen"] != "pass":
            reasons.append("DRM image absent or black/static")
        if row["audio"] != "pass":
            reasons.append("ALSA playback pointer did not advance")
        lines.append(
            f"- `{row['system']}` / `{row['profile']}`: {', '.join(reasons)}"
        )
    lines.extend(["", "## Not exercised (no cached ROM)", ""])
    lines.append(
        ", ".join(f"`{system}`" for system in report["untested_systems"])
        if report["untested_systems"]
        else "None."
    )
    lines.extend(["", "## Always manual", ""])
    lines.extend(
        [
            "- 画面の向き、アスペクト比、欠け、色、ちらつき",
            "- 実際に聞こえる音、音質、音飛び、映像との同期",
            "- 入力、メニュー、終了、セーブ/ロード",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--user", default="root")
    parser.add_argument("--password", default=os.environ.get("PLUMOS_SSH_PASSWORD", ""))
    parser.add_argument(
        "--capture-tool",
        default="output/live/2026-08-16-dreamcast-rotation/drm-scanout-capture-v2",
    )
    parser.add_argument("--seconds", type=int, default=8)
    parser.add_argument("--systems", default="")
    parser.add_argument("--all-profiles", action="store_true")
    parser.add_argument(
        "--skip-default-profile",
        action="store_true",
        help="with --all-profiles, exercise only non-default profiles",
    )
    parser.add_argument("--output", default="")
    args = parser.parse_args()

    stamp = dt.datetime.now().strftime("%Y-%m-%d-%H%M%S")
    output_dir = Path(args.output or f"output/validation/pixel2-device-media-{stamp}")
    output_dir.mkdir(parents=True, exist_ok=True)
    device = Device(args.host, args.user, args.password)
    capture_tool = Path(args.capture_tool)
    if not capture_tool.is_file():
        raise SystemExit(f"capture tool missing: {capture_tool}")
    device.copy_to(capture_tool, "/tmp/drm-scanout-capture")
    device.run("chmod 0755 /tmp/drm-scanout-capture")

    inventory_result = device.run("/usr/bin/python3 -", input_text=REMOTE_INVENTORY)
    inventory = json.loads(inventory_result.stdout)
    requested = {item.strip() for item in args.systems.split(",") if item.strip()}
    candidates = [
        row
        for row in inventory
        if row["roms"] and (not requested or row["id"] in requested)
    ]
    untested = [
        row["id"]
        for row in inventory
        if not row["roms"] and (not requested or row["id"] in requested)
    ]
    results = []
    try:
        for index, row in enumerate(candidates, 1):
            rom = select_rom(row["roms"])
            profiles = row["profiles"] if args.all_profiles else [row["default_profile"]]
            profiles = [profile for profile in profiles if profile]
            if args.skip_default_profile:
                profiles = [
                    profile
                    for profile in profiles
                    if profile != row["default_profile"]
                ]
            for profile in profiles:
                print(
                    f"[{index}/{len(candidates)}] {row['id']} {profile}: {rom}",
                    flush=True,
                )
                result = launch_one(
                    device,
                    output_dir,
                    row["id"],
                    profile,
                    rom,
                    args.seconds,
                )
                results.append(result)
                print(
                    f"  startup={result['startup']} screen={result['screen']} "
                    f"audio={result['audio']} battery={result['battery_percent_after']}%",
                    flush=True,
                )
    finally:
        device.run(
            "/mnt/plumos/bin/plumos-frontend-stop stop >/dev/null 2>&1 || true; "
            "/mnt/plumos/bin/plumos-frontend-launch >/dev/null 2>&1 || true",
            check=False,
            timeout=20,
        )

    report = {
        "generated": dt.datetime.now(dt.timezone.utc).astimezone().isoformat(),
        "device": f"{args.user}@{args.host}",
        "results": results,
        "untested_systems": untested,
    }
    json_path = output_dir / "report.json"
    md_path = output_dir / "report.md"
    json_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    write_markdown(md_path, report)
    print(md_path)
    return 1 if any(row["overall"] == "fail" for row in results) else 0


if __name__ == "__main__":
    sys.exit(main())
