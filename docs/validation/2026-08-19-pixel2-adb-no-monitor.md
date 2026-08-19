# Pixel2 explicit-only ADB lifecycle

Date: 2026-08-19

## Why the monitor was removed

The first Pixel2 ADB implementation (`10fc87a`) configured FunctionFS and
started `adbd` once. It did not include a USB-state monitor. The first
stabilisation (`6f022d4`) added only a daemon-existence watchdog; it did not
judge host attachment, protocol state, or kernel disconnect events.

The kernel uevent listener was introduced later. Live evidence showed that a
normal macOS enumeration could briefly create an ADB transport and then emit a
disconnect transition. The listener treated that transition as a failure,
reset FunctionFS/DWC2, and destroyed the transport it was trying to recover.
Further disconnect events then amplified the same cycle until the frontend
reported `waiting` or `STOP`.

Commit `dacbc83` removes both automatic monitor paths:

- `/usr/lib/plumos/init.d/15-adbd-watchdog`;
- `/usr/lib/plumos/adb-uevent`.

`10-adbd start` now performs one FunctionFS/adbd setup. A DWC2 reset is allowed
only for the explicit `restart` action used by USB Mode changes and resume.
Cable events and host events do not trigger autonomous recovery.

## Build and package verification

The System rootfs, frontend, network-services component, and strict app-layer
were rebuilt as `0.1.0-dev-dacbc83`. The strict app-layer initially exposed a
stale frontend component checksum for the shared `bin/plumos-network-services`
file. Rebuilding the frontend component restored atomic root and component
metadata, after which verification passed.

The live exact bases were read before package creation:

- System: `0.1.0-dev-626a1e8`;
- Runtime: `0.1.0-dev-f5ca09e`;
- Runtime checksum-list SHA-256:
  `48350318981d9f4f7f76c0c2d3c01dabafd9fa4af7689893c8942217f9dbcaaf`.

The signed packages were inspected by the production updater before apply:

- System SHA-256:
  `13d52ed0b8f518bb3e944aedff5f9726d78809cae8f4ede0bbaaaba3a3a19c98`;
- Runtime SHA-256:
  `7c462e52d3cc352289b5c9b31e8cd6ec8f6bbc0625bb66889214328daaa28bf2`;
- Runtime payload: 12 managed entries, zero deletions.

System and Runtime were applied in separate healthy update transactions. Live
readback confirmed both versions as `0.1.0-dev-dacbc83`, `10-adbd` SHA-256 as
`e51bda7b7c7e7021c16575157ceade6a0678ea018359599952a2ccbf96abf28d`,
both monitor files and processes absent, and full Runtime verification
`result-ok`. ROMs, BIOS, saves, settings, installed ports, and older update
inbox entries were not modified.

## Physical acceptance

The device was switched from Wi-Fi to ADB through the explicit USB Mode action.
The remaining checks require the Mac cable:

1. the first cable connection reaches a real `adb shell`;
2. the transport stays present without a monitor-triggered reset;
3. cable removal and insertion has a deterministic result;
4. if re-enumeration needs recovery, an explicit Off -> ADB transition works
   without an autonomous listener.
