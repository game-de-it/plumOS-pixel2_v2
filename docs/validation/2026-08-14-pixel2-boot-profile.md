# Pixel2 boot-time profile

Date: 2026-08-14

Device runtime: `0.1.0-dev-9410f72`

## Scope

This investigation profiles the already-running physical Pixel2 without
rebooting or changing the installed runtime. It combines the current kernel
monotonic log, RTC-backed file timestamps, service logs, and isolated read-only
timings of the same commands used during boot. Power-on-to-kernel time in the
stock Rockchip bootloader is not included.

## Observed timeline

| Point | Kernel / derived time | Elapsed from kernel start |
| --- | ---: | ---: |
| kernel runs stock `/init` | 1.431 s | 1.431 s |
| stock initramfs mounts `/flash/SYSTEM` | 2.318 s | 2.318 s |
| dispatcher mounts ready | 2.879 s | 2.879 s |
| active System slot selected | 4.029 s | 4.029 s |
| plumOS `/sbin/init` starts | 4.515 s | 4.515 s |
| Panfrost ready | 9.399 s | 9.399 s |
| ADB gadget configured | 11.755 s | 11.755 s |
| frontend writes renderer-ready proof | derived 67.287 s | 67.287 s |

The renderer-ready timestamp is derived from `/tmp/plumos-fe-ready` mtime
`2026-08-13 18:11:14.573339972 UTC` and the kernel RTC synchronization at
uptime 0.714 s to `2026-08-13 18:10:08 UTC`.

## Primary cause

`rootfs/pixel2/usr/lib/plumos/init.d/40-frontend` verifies the complete runtime
before starting the frontend:

```sh
(cd "$APP_ROOT" && busybox sha256sum -c checksums.sha256)
```

The current app layer is 1.2 GiB on disk. Its root checksum list contains 3490
files and 1110.1 MiB of file payload. Running the exact verification on the
device took:

```text
real 52.60
user 46.40
sys  3.14
```

This single gate accounts for about 78% of the measured 67.3-second
kernel-to-frontend interval. CPU time dominates, so the delay is primarily the
RK3326 hashing the whole runtime rather than ROM scanning or a blocked device
driver.

The largest hashed groups are:

| Group | Files | Payload |
| --- | ---: | ---: |
| `cores` | 112 | 452.7 MiB |
| `apps` | 2248 | 231.3 MiB |
| `emulator` | 51 | 194.6 MiB |
| `standalone` | 528 | 123.4 MiB |
| `lib` | 92 | 68.6 MiB |

The full checksum was introduced when the app layer was still a small NES
baseline (`3a7e80a`). It now scales with every core, standalone emulator, and
Pyxel runtime added to the image.

## Secondary costs

- The no-request Python updater path costs 2.33 seconds with warm cache and
  4.30 seconds on the first isolated run. The boot log shows `apply-pending`
  spanning approximately five wall-clock seconds even though there is no update
  request. It imports the complete updater and recreates the staging directory
  before checking whether `request.json` exists.
- The dispatcher verifies the 23.7 MiB System slot at every boot. The isolated
  SHA-256 took 1.16 seconds and matches the 1.15-second interval between
  dispatcher mounts-ready and slot-selected. This is bounded and materially
  smaller than the runtime checksum.
- The frontend ROM-library scan took only 174 ms on the current card. It is not
  the source of the long boot.
- `plumos-diagnostics` took 0.03 seconds and `mdev -s` took 0.05 seconds when
  isolated. USB Wi-Fi and SSH return immediately because they are not
  configured.

## Comparison with sibling plumOS implementations

The MF boot hook checks that the frontend component manifest and checksum
metadata exist before starting its supervisor; it does not hash the entire app
layer in the synchronous frontend path. MF/V90S perform complete checks during
build/deployment/update validation. Pixel2 therefore added a stricter normal
boot gate that became disproportionately expensive as its app layer grew.

## Recommended correction

Do not remove integrity verification without replacing its trust boundary.
The Pixel2 update contract already requires a signed package, transactional
apply, complete checksum verification before reboot, and renderer-ready health
promotion. A compatible correction should:

1. keep complete root/component checksum verification at build, deployment,
   and signed update apply time;
2. on an unchanged healthy generation, synchronously validate only critical
   metadata and frontend/bootstrap files before drawing the UI;
3. run the complete 1.1 GiB audit after renderer-ready or on explicit Storage /
   Runtime Health request, recording a failure for the next controlled recovery;
4. add a shell fast path that skips Python updater startup when no request,
   runtime-pending state, or interrupted journal exists;
5. retain the small System-slot checksum before mounting it;
6. add monotonic timestamps around every init service and frontend readiness so
   subsequent boot regressions are automatically attributable.

This investigation diagnoses the delay only; no boot behavior has been changed.
