# Pixel2 Frontend Environment Isolation

Date: 2026-08-28  
Device: GKD Pixel2  
Source: `508b567`

## Symptom

After Rockbox was launched from the frontend and exited normally, PFS started
from the Pyxel system with working audio but an all-black display. The Pyxel
process remained alive and the ALSA hardware pointer advanced, while the active
480x640 DRM plane contained exactly one black colour.

This was order dependent. PFS and Last Emulator had already passed their direct
Pixel2 launch routes, so a standalone Pyxel display regression did not explain
the failure.

## Root cause

The PortMaster port launcher restored the frontend from its own exported
environment. That frontend inherited Rockbox and PortMaster display adapters in
`LD_PRELOAD`; a later Pyxel launch inherited them again and appended its two
Pyxel adapters. The failing PFS process therefore contained all of these
families at once:

```text
plumos-pyxel-fit.so
plumos-pyxel-gl-rotate.so
libplumos-portmaster-exec-guard.so
libplumos-portmaster-sdl-rotate.so
libplumos-portmaster-gl-rotate.so
libplumos-portmaster-rockbox.so
```

The frontend was acting as an unintended carrier for a previous application's
private renderer state.

## Fix

`plumos-frontend-launch` now discards inherited loader state and starts the
frontend init script through a minimal `env -i` environment containing only
the system path, plumOS root, and runtime root. Frontend init remains responsible
for publishing the Pixel2 input, renderer, ROM, BIOS, and device contract.

`tests/test-pixel2-frontend-clean-environment.sh` poisons loader, SDL,
PortMaster, and Pyxel variables and proves that none reaches the fake frontend
init. It is part of the normal app-layer script suite.

## Verification

Host checks passed:

```text
pixel2_frontend_clean_environment=result-ok
portmaster_pixel2_runtime=result-ok
portmaster_pixel2_rockbox=result-ok
portmaster_pixel2_session_cleanup=result-ok
app_layer_verify=result-ok strict=1
```

The exact-source signed Runtime delta was applied through the normal updater:

```text
runtime=0.1.2-dev-508b567
package_sha256=cddeafd5c070448e2d18fc8802a64955bf1b276938a1eea5b888d078ccd97c57
payload_files=10
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
```

After the fix, the restored frontend had no `LD_PRELOAD` and no
`PLUMOS_PORTMASTER_*` variables. PFS then loaded only the two expected Pyxel
adapters. Its active DRM plane was no longer black, ALSA remained `RUNNING` and
advanced, and the operator confirmed the display on the physical LCD.

Evidence hashes:

```text
9347fe86f6b448518e30d671057b4ece3e7f2cf3906488168f63736a4c990bc0  pfs-black-logical.png
1958ca95e4c0b8d46a7472f3fb2d3c5702e83f3b71fe50db6ffd7681e9fa9398  pfs-clean-logical.png
```

This repair is intentionally generic: any emulator or application launched
after a PortMaster title receives its own launch profile, not the previous
port's private display libraries.
