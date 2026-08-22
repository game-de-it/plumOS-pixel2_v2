# Pixel2 PicoArch FBNeo Function menu recovery

Date: 2026-08-22

## Symptom

The physical Function button did not open PicoArch's menu while the FBNeo core
was running. The same raw Pixel2 `BTN_TRIGGER_HAPPY1` binding had already been
accepted with other PicoArch cores.

## Root cause

The live device ran PicoArch with `fbneo_libretro.so` and held
`/dev/input/event2`. Its environment still contained the correct raw Function
code 704. Inspection of the pinned FBNeo source showed that its `retro_run()`
never calls the registered `input_poll_cb()`. PicoArch previously updated both
game input and `EACTION_MENU` only from that callback, so the Function event was
never evaluated on this core.

## Fix

PicoArch now tracks whether the core invoked input polling in each frame. A
compliant core keeps the existing callback path unchanged. Only when a core
returns from `retro_run()` without polling does the frontend perform one
fallback poll. This fixes FBNeo without double-polling compliant cores and also
covers any future core with the same libretro contract violation.

The first fallback emits one diagnostic line:

```text
Core omitted input_poll_cb; enabling frontend fallback polling
```

## Host verification

- Pixel2 Function/menu contract: PASS
- app-layer script suite: PASS
- clean AArch64 PicoArch component build: PASS
- PicoArch component checksums: PASS
- generated binary contains the fallback diagnostic: PASS

## Live deployment

A signed Runtime delta was generated from the installed
`0.1.0-dev-f49d7ed` checksum inventory to `0.1.0-dev-f41653e`. The package
contained no deletions. The device updater accepted the signature, exact source
version, device/ABI contract, and package SHA-256
`f28e451d6516ab253787456295f7c1c27e0db2381e92a3f9bf65b9b35c1eaedc`.

After the normal update reboot:

- Runtime version and `source_ref` were `0.1.0-dev-f41653e` and `f41653e`;
- live PicoArch SHA-256 matched the host artifact at
  `0bee39143ddbf3ff26f3d6194644d86a1866727850c4a106cf7a0b09e0b786d4`;
- `plumos-system-update verify-runtime` returned `runtime_verify=result-ok`;
- FBNeo `jackal-arcade.zip` launched through the normal PicoArch profile.

Physical Function/menu acceptance remains pending.
