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

Real-device deployment and physical Function/menu acceptance remain pending.
