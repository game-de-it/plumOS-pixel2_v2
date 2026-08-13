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
