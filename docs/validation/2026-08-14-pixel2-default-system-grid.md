# Pixel2 default system grid

Date: 2026-08-14

## Problem

The Pixel2 initial frontend port made the only bundled `default` directory
contain a theme identified as `default-horizontal`. Its top-level system picker
used `tile_strip`, showing two icons in one row and accepting left/right
navigation. This differed from the plumOS V90S default presentation.

## Decision

Pixel2 now uses the shared plumOS default graphic contract:

- theme id: `default`;
- layout preset: `grid_preview`;
- system layout: `tile_grid`;
- grid size: 3 columns by 2 rows, six systems per page;
- page transition axis: vertical.

The frontend already implements both `tile_grid` and the optional
`tile_strip`; this change selects the six-icon grid as the default without
removing the user's explicit Top Layout setting.

## Existing settings

The inspected device's `/mnt/plumos/config/frontend/settings.json` has an empty
`graphic_top_layout` override, so replacing the managed default theme changes
it to the six-icon grid without modifying user settings. Explicit user layout
choices remain preserved.

## Host validation

The implementation was committed as `c808952` and rebuilt with Runtime version
`0.1.0-dev-c808952`:

```text
tests/test-app-layer-scripts.sh: result-ok
tests/test-implementation-audit.sh: release_blockers=0
frontend_component=result-ok
app_layer=result-ok strict=1
```

The pre-deployment checksum comparison found only the intended managed Runtime
changes: the default theme, Runtime/frontend manifests and checksum metadata,
and `VERSION`.

## Device deployment

Before deployment the installed Runtime passed `3490/3490` checksums. A signed
delta Runtime package was generated and read back from the device with matching
SHA-256:

```text
package=plumos-pixel2-runtime-0.1.0-dev-c808952.tar.gz
sha256=bcf396b174e613f9813d699857635e9ae8b9da8997cbef3b2ff195dafc8dbcc9
source_version=0.1.0-dev-9410f72
version=0.1.0-dev-c808952
```

The transactional updater completed its update-time full Runtime verification,
the frontend returned, and renderer-ready promoted the transaction to:

```text
result=runtime_healthy
status=healthy
version=0.1.0-dev-c808952
```

The live managed theme reports `id=default`, `top_layout=tile_grid`, and
`transition_axis=vertical`; the mutable frontend settings remain unchanged with
an empty layout override. `/dev/fb0` still exposes the stale stock splash buffer
while the frontend is active, so it cannot provide visual proof of the DRM/live
display. The six visible tiles and physical D-pad navigation remain a direct LCD
acceptance check.
