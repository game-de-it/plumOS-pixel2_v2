# Pixel2 v0.1.3 PortMaster update regression

Date: 2026-08-30  
Implementation commit: `1d1195e`

## Report

Users reported a black Rockbox screen and failure to launch Blaze of Storm
after updating to v0.1.3. The public v0.1.3 update archive was downloaded again
and matched the local release artifact exactly. Its payload contains the
Rockbox renderer library, common PortMaster launcher, frontend environment
isolation, and matching app-layer metadata. An updated device also read back
the exact public v0.1.3 hashes, so the report was not caused by an omitted
release payload.

## Root cause

v0.1.3 ran the generic compatibility audit synchronously before handing the
display to every port. Rockbox required about six seconds to inspect 203 ELF
files and the shared library roots. The user saw only a black transition frame
during that interval. A larger commercial port can take longer.

If that launch was interrupted, the old cleanup removed
`/run/plumos/portmaster/port.mounts` even when an unmount failed. Patcher,
configuration, library, theme, compatibility, or Rockbox bind mounts could
therefore remain active without a tracking file. Every later port was then
rejected as a stale compatibility mount. This explains why two unrelated
ports could fail together after the update.

## Repair

Adapter 50 changes the common PortMaster boundary rather than patching either
title:

- the port process starts before the compatibility audit;
- the audit runs as a detached advisory task after the port exits;
- normal unmount is retried five times;
- known private PortMaster mounts are recovered even if tracking was lost;
- a still-busy private mount is lazily detached after the bounded retries;
- the tracking file is removed only after the mounts are confirmed absent.

The allowlist restricts recovery to the Pixel2 PortMaster private mounts and
per-session home mounts. Arbitrary host paths cannot be detached.

## Host evidence

The PortMaster component and complete app layer were rebuilt from `1d1195e`.
Component checksums, the app-layer verifier, shell syntax, and the following
tests passed:

```text
portmaster_pixel2_mount_cleanup=result-ok
portmaster_pixel2_audit=result-ok
portmaster_pixel2_session_cleanup=result-ok
portmaster_pixel2_runtime=result-ok
```

The signed v0.1.3 transition is:

```text
source_version=0.1.3
version=0.1.3-dev-1d1195e
payload_files=15
deleted_files=0
package_sha256=a6c1e278f4a9e5322fb14d32404ff52009fee9c87ffe8ec7b988a851f91b5c3b
```

The device verified the package hash before and after atomic rename. The
production updater accepted its signature and source chain and promoted it as
`runtime_healthy`. Full managed-runtime verification passed after reboot.

## Device evidence

Rockbox created its launcher PID, GPTokeYB2 process, and `rockbox` process in
less than the first one-second observation window, before the former six-second
audit could complete. The captured DRM frame is upright, 640x480, and visibly
contains the Rockbox main menu rather than a black frame:

```text
8eb01bc74a04064a17303828378b4662a40af7b69f8d6eaa78fcc96bf4e18a95  rockbox-hotfix-logical.png
```

On exit, the audit completed asynchronously with 203 ELF files, zero errors,
and the existing private-`LD_PRELOAD` warning. The frontend returned as one
process; no Rockbox, PortMaster launcher, GPTokeYB, compatibility mount, or
mount tracking file remained. A deliberately untracked patcher bind-mount
fixture was also recovered by the new helper.

Blaze of Storm is not installed on the laboratory device. Tiny Rally was used
as the installed GameMaker/`gmloadernext.aarch64` representative: the common
launcher started the runtime without waiting for audit enforcement, then
cleanly recovered its process group, mounts, and frontend. Its automated DRM
capture remained black, so this is process/lifecycle evidence only, not visual
acceptance for that title. Blaze of Storm display, input, and audio remain a
user-side retest boundary with the actual game data.

## Preservation boundary

The transaction updated only managed app-layer files and their checksum and
manifest entries. Installed PortMaster content, game data, settings, saves,
ROMs, BIOS files, credentials, and Wi-Fi configuration were not replaced.
