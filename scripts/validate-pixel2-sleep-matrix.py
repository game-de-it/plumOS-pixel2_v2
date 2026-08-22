#!/usr/bin/env python3
"""Mechanically validate Pixel2 sleep/resume across runtime families.

This exercises the production power overlay ownership handoff while replacing
only its physical menu selection with a bounded automatic Sleep selection.
Screen proof means a panel-sized, non-black/non-static DRM plane exists.  Audio
proof means the ALSA playback pointer advances after resume; it is not a claim
about audible quality, sync, orientation, controls, or visual correctness.
"""

from __future__ import annotations

import argparse
import datetime as dt
import importlib.util
import json
from pathlib import Path
import shlex
import time
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
MEDIA_VALIDATOR = ROOT / "scripts/validate-pixel2-device-media.py"
spec = importlib.util.spec_from_file_location("pixel2_device_media", MEDIA_VALIDATOR)
if spec is None or spec.loader is None:
    raise SystemExit(f"cannot load {MEDIA_VALIDATOR}")
media = importlib.util.module_from_spec(spec)
spec.loader.exec_module(media)


CASES: dict[str, dict[str, str]] = {
    "ra": {
        "family": "RetroArch",
        "system": "fds",
        "profile": "retroarch:fceumm",
        "rom": "FDS/Akumajou Dracula.zip",
    },
    "sa": {
        "family": "Standalone",
        "system": "psp",
        "profile": "standalone:ppsspp",
        "rom": "PSP/Telegraph Crosswords.cso",
    },
    "pico": {
        "family": "PicoArch",
        "system": "fbneo",
        "profile": "picoarch:fbneo",
        "rom": "FBNEO/jackal-arcade.zip",
    },
    "apps": {
        "family": "Apps / Music Player",
        "system": "apps",
        "profile": "app:music-player",
        "rom": "Music/plumos-sleep-test.mp3",
    },
}


def launch_command(case: dict[str, str]) -> str:
    common = (
        "PLUMOS_ROOT=/mnt/plumos PLUMOS_SDCARD_ROOT=/mnt/plumos-user "
        "PLUMOS_RUNTIME_ROOT=/run/plumos"
    )
    if case["profile"] == "app:music-player":
        return (
            f"exec env {common} PLUMOS_MUSIC_AUTOPLAY=1 "
            "PLUMOS_MUSIC_EXIT_AFTER_MS=0 "
            "/mnt/plumos/bin/plumos-music-player-launch"
        )
    return (
        f"exec env {common} /mnt/plumos/bin/plumos-text-ui launch "
        f"{shlex.quote(case['system'])} {shlex.quote(case['rom'])} "
        f"--profile {shlex.quote(case['profile'])} --execute --no-scan"
    )


def live_group(ps_text: str, pgid: int) -> list[dict[str, str]]:
    rows = []
    for line in ps_text.splitlines():
        columns = line.split(None, 5)
        if len(columns) < 5 or columns[2] != str(pgid):
            continue
        if columns[3].startswith("Z"):
            continue
        rows.append(
            {
                "pid": columns[0],
                "ppid": columns[1],
                "pgid": columns[2],
                "state": columns[3],
                "command": columns[4],
                "args": columns[5] if len(columns) > 5 else "",
            }
        )
    return rows


def audio_advances(first: dict[str, Any], second: dict[str, Any]) -> bool:
    return (
        first.get("state") == "RUNNING"
        and second.get("state") == "RUNNING"
        and isinstance(first.get("hw_ptr"), int)
        and isinstance(second.get("hw_ptr"), int)
        and first["hw_ptr"] != second["hw_ptr"]
    )


def collect_remote(device: Any, prefix: str, phase: str) -> None:
    target = f"{prefix}-{phase}"
    last_error: Exception | None = None
    for _ in range(10):
        try:
            device.run(
                f"ps -eo pid,ppid,pgid,stat,comm,args >{target}.ps; "
                f"/tmp/drm-scanout-capture /dev/dri/card0 {target} "
                f"2>{target}.capture.log || true; "
                f"cat /proc/asound/card0/pcm0p/sub0/status >{target}.audio1 2>&1 || true; "
                "sleep 1; "
                f"cat /proc/asound/card0/pcm0p/sub0/status >{target}.audio2 2>&1 || true",
                timeout=20,
            )
            return
        except Exception as exc:
            last_error = exc
            time.sleep(3)
    raise RuntimeError(f"evidence collection failed after reconnect: {last_error}")


def copy_evidence(device: Any, prefix: str, output: Path) -> None:
    listing = None
    for _ in range(10):
        try:
            listing = device.run(
                f"for f in {prefix}*; do [ -f \"$f\" ] && printf '%s\\n' \"$f\"; done",
                check=False,
                timeout=8,
            )
            break
        except Exception:
            time.sleep(3)
    if listing is None:
        raise RuntimeError("evidence listing failed after reconnect")
    files = listing.stdout.splitlines()
    for remote in files:
        for attempt in range(10):
            try:
                device.copy_from(remote, output / Path(remote).name)
                break
            except Exception:
                if attempt == 9:
                    raise
                time.sleep(3)


def stop_case(device: Any, pid: int, profile: str) -> None:
    if profile == "app:music-player":
        device.run(
            f"kill -TERM -{pid} 2>/dev/null || true; sleep 2; "
            f"kill -KILL -{pid} 2>/dev/null || true",
            check=False,
            timeout=8,
        )
        return
    media.stop_launch(device, pid, profile)


def run_case(
    device: Any,
    output: Path,
    case_id: str,
    case: dict[str, str],
    startup_seconds: int,
    sleep_seconds: int,
) -> dict[str, Any]:
    base = f"plumos-sleep-{case_id}"
    prefix = f"/tmp/{base}"
    command = launch_command(case)
    device.run(
        "/mnt/plumos/bin/plumos-frontend-stop stop >/dev/null 2>&1 || true; "
        f"setsid /bin/sh -c {shlex.quote(command)} "
        f">{prefix}.launch.log 2>&1 </dev/null & echo $! >{prefix}.pid"
    )
    pid_text = device.run(f"cat {prefix}.pid").stdout.strip()
    if not pid_text.isdigit():
        raise RuntimeError(f"{case_id}: launcher PID missing")
    pid = int(pid_text)
    try:
        time.sleep(startup_seconds)
        collect_remote(device, prefix, "pre")
        pre_audio1_live = media.parse_audio(
            device.run(f"cat {prefix}-pre.audio1", timeout=8).stdout
        )
        pre_audio2_live = media.parse_audio(
            device.run(f"cat {prefix}-pre.audio2", timeout=8).stdout
        )
        if not audio_advances(pre_audio1_live, pre_audio2_live):
            raise RuntimeError(
                f"{case_id}: audio was not running before sleep "
                f"({pre_audio1_live.get('state')} -> {pre_audio2_live.get('state')})"
            )
        device.run(
            f"/bin/busybox setsid /bin/sh /tmp/pixel2-sleep-cycle-device "
            f"{prefix} {sleep_seconds} >{prefix}.cycle.log 2>&1 </dev/null &"
        )
        # A real kernel suspend drops the Wi-Fi-backed SSH session. Poll with
        # fresh connections instead of treating that disconnect as a failed
        # sleep cycle.
        deadline = time.monotonic() + sleep_seconds + 60
        complete = False
        while time.monotonic() < deadline:
            try:
                probe = device.run(
                    f"test -e {prefix}.sleep-done && echo done || true",
                    check=False,
                    timeout=6,
                )
                if probe.stdout.strip() == "done":
                    complete = True
                    break
            except Exception:
                pass
            time.sleep(2)
        if not complete:
            raise RuntimeError(f"{case_id}: sleep cycle did not complete")
        time.sleep(2)
        collect_remote(device, prefix, "post")
        copy_evidence(device, prefix, output)

        pre_ps = (output / f"{base}-pre.ps").read_text(errors="replace")
        post_ps = (output / f"{base}-post.ps").read_text(errors="replace")
        pre_live = live_group(pre_ps, pid)
        post_live = live_group(post_ps, pid)
        pre_capture = media.analyze_capture(output, f"{base}-pre")
        post_capture = media.analyze_capture(output, f"{base}-post")
        pre_audio1 = media.parse_audio(
            (output / f"{base}-pre.audio1").read_text(errors="replace")
        )
        pre_audio2 = media.parse_audio(
            (output / f"{base}-pre.audio2").read_text(errors="replace")
        )
        post_audio1 = media.parse_audio(
            (output / f"{base}-post.audio1").read_text(errors="replace")
        )
        post_audio2 = media.parse_audio(
            (output / f"{base}-post.audio2").read_text(errors="replace")
        )
        sleep_rc = (output / f"{base}.sleep-rc").read_text().strip()
        power_log = (output / f"{base}.power.log").read_text(errors="replace")
        overlay_log = (output / f"{base}.overlay.log").read_text(errors="replace")
        paused = "pause source=" in overlay_log and any(
            row["pid"] in overlay_log for row in pre_live
        )
        sleep_complete = (
            sleep_rc == "0"
            and "sleep=display-blank" in power_log
            and "sleep=display-unblank" in power_log
            and "sleep=result-returned" in power_log
        )
        checks = {
            "startup": bool(pre_live),
            "screen_before": pre_capture["machine_visible"],
            "audio_before": audio_advances(pre_audio1, pre_audio2),
            "paused_during_sleep": paused,
            "sleep_complete": sleep_complete,
            "same_process_group_resumed": bool(post_live),
            "screen_after": post_capture["machine_visible"],
            "audio_after": audio_advances(post_audio1, post_audio2),
        }
        return {
            "id": case_id,
            **case,
            "pid": pid,
            "checks": checks,
            "overall": "pass" if all(checks.values()) else "manual",
            "pre_processes": pre_live,
            "post_processes": post_live,
            "pre_capture": pre_capture,
            "post_capture": post_capture,
            "pre_audio": [pre_audio1, pre_audio2],
            "post_audio": [post_audio1, post_audio2],
            "power_log": power_log.rstrip(),
            "overlay_log": overlay_log.rstrip(),
        }
    finally:
        stop_case(device, pid, case["profile"])


def write_report(path: Path, report: dict[str, Any]) -> None:
    lines = [
        "# Pixel2 sleep/resume machine matrix",
        "",
        f"- generated: `{report['generated']}`",
        f"- device: `{report['device']}`",
        f"- automatic sleep interval: `{report['sleep_seconds']} seconds`",
        "",
        "| family | route | startup | paused | sleep | screen pre/post | audio pre/post | result |",
        "|---|---|---:|---:|---:|---:|---:|---:|",
    ]
    for row in report["results"]:
        checks = row["checks"]
        screen = checks["screen_before"] and checks["screen_after"]
        lines.append(
            f"| {row['family']} | `{row['profile']}` | "
            f"{'PASS' if checks['startup'] else 'FAIL'} | "
            f"{'PASS' if checks['paused_during_sleep'] else 'FAIL'} | "
            f"{'PASS' if checks['sleep_complete'] and checks['same_process_group_resumed'] else 'FAIL'} | "
            f"{'PASS' if screen else 'MANUAL'} | "
            f"{'PASS' if checks['audio_before'] and checks['audio_after'] else 'MANUAL'} | "
            f"**{row['overall'].upper()}** |"
        )
    lines.extend(
        [
            "",
            "## Scope boundary",
            "",
            "- PASS for screen means a panel-sized non-black/non-static DRM plane existed before and after resume.",
            "- PASS for audio means the ALSA hardware pointer advanced after resume.",
            "- Orientation, aspect, flicker, audible quality/sync, controls, and physical Power-menu behavior remain operator checks.",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--user", default="root")
    parser.add_argument("--password", default="")
    parser.add_argument("--cases", default="ra,sa,pico,apps")
    parser.add_argument("--startup-seconds", type=int, default=10)
    parser.add_argument("--sleep-seconds", type=int, default=3)
    parser.add_argument(
        "--capture-tool",
        default="output/live/2026-08-16-dreamcast-rotation/drm-scanout-capture-v2",
    )
    parser.add_argument("--output", default="")
    args = parser.parse_args()
    selected = [item.strip() for item in args.cases.split(",") if item.strip()]
    unknown = sorted(set(selected) - set(CASES))
    if unknown:
        parser.error("unknown cases: " + ", ".join(unknown))
    if args.sleep_seconds < 1 or args.startup_seconds < 1:
        parser.error("sleep/startup seconds must be positive")

    stamp = dt.datetime.now().strftime("%Y-%m-%d-%H%M%S")
    output = Path(args.output or f"output/validation/pixel2-sleep-{stamp}")
    output.mkdir(parents=True, exist_ok=True)
    device = media.Device(args.host, args.user, args.password)
    capture = Path(args.capture_tool)
    if not capture.is_file():
        raise SystemExit(f"capture tool missing: {capture}")
    device.copy_to(capture, "/tmp/drm-scanout-capture")
    device.run("chmod 0755 /tmp/drm-scanout-capture")
    selector = ROOT / "scripts/pixel2-sleep-auto-select.sh"
    device.copy_to(selector, "/tmp/pixel2-sleep-auto-select")
    device.run("chmod 0755 /tmp/pixel2-sleep-auto-select")
    cycle = ROOT / "scripts/pixel2-sleep-cycle-device.sh"
    device.copy_to(cycle, "/tmp/pixel2-sleep-cycle-device")
    device.run("chmod 0755 /tmp/pixel2-sleep-cycle-device")
    # Preserve an existing test track.  Only seed one when Music Player has no
    # recognized audio available, using content already present on the card.
    device.run(
        "mkdir -p /mnt/plumos-user/Music; "
        "target=/mnt/plumos-user/Music/plumos-sleep-test.mp3; "
        "size=$(stat -c %s \"$target\" 2>/dev/null || echo 0); "
        "if [ \"$size\" -lt 1048576 ]; then "
        "source=/mnt/plumos-user/roms/pyxel/LastEmulator_assets/assets/sounds/bgm/BGM_16.mp3; "
        "[ -f \"$source\" ] || source=$(find /mnt/plumos-user/roms/pyxel -type f -name '*.mp3' 2>/dev/null | head -n 1); "
        "[ -n \"$source\" ] && cp \"$source\" \"$target\" || true; fi"
    )

    results = []
    try:
        for case_id in selected:
            print(f"[{case_id}] launch/sleep/resume", flush=True)
            row = run_case(
                device,
                output,
                case_id,
                CASES[case_id],
                args.startup_seconds,
                args.sleep_seconds,
            )
            results.append(row)
            print(f"  result={row['overall']} checks={row['checks']}", flush=True)
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
        "sleep_seconds": args.sleep_seconds,
        "results": results,
    }
    (output / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    write_report(output / "report.md", report)
    print(output / "report.md")
    return 1 if any(row["overall"] != "pass" for row in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())
