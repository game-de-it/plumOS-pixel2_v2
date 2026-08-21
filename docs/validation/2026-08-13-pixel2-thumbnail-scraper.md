# Pixel2 thumbnail scraper

## Ownership and route

- Apps exposes `Scraping` through `internal:scraping`;
- `plumos-thumbnail-scraper` owns plan, fetch, result, cache, and atomic PNG
  replacement;
- content is read from `/mnt/plumos-user/roms` and artwork is written below
  `/mnt/plumos-user/Images`;
- HTTPS uses component-owned curl, recursive runtime dependencies, loader, and
  CA bundle; stock userspace commands and certificates are not required;
- scraper logs contain counters and stages, not ROM filenames or credentials.

## Host evidence

- frontend component build: pass;
- strict app-layer build: pass;
- owned aarch64 curl `--version`: pass;
- all-system policy parsing against the Pixel2 multi-line JSON catalog: pass;
- NES plan with empty fixture: pass;
- NES fetch using `Clu Clu Land.nes` from the provided ROM set: one candidate,
  one CRC checked, one CRC match, one PNG downloaded, zero failures;
- implementation audit release blockers: 5 before this unit, 4 after it.

## 2026-08-21 parallel bootstrap repair

The first full NES run on Pixel2 found 118 candidates but downloaded nothing.
Both CRC workers independently tried to populate the same DAT and thumbnail
listing cache over Wi-Fi, then both logged `missing_index_or_source`; the final
result counted all 118 entries as `skipped_tool`.

- `7ee72d7` serializes shared DAT and thumbnail index creation and retries all
  curl transport errors;
- `c1cd822` completes the primary system index preflight in the parent before
  CRC workers start. The lock remains as protection for alternate payload
  systems found inside archives;
- the regression fixture starts two CRC workers with an empty cache, delays
  both shared sources, verifies one DAT request and one listing request, then
  verifies two successful PNG writes;
- `tests/test-app-layer-scripts.sh`, the dedicated parallel fixture, and the
  Pixel2 frontend component build passed.

On device `192.168.10.137`, the empty-cache two-worker acceptance produced two
CRC matches, two downloads, zero download failures, and zero tool skips. The
full NES pass scanned 118 ROMs, retained two existing images, downloaded 105
more, and ended with five CRC misses plus six thumbnail-name misses. No PNG
download failed. A thumbnail-aware frontend library scan resolved 107 of 118
ROM entries in 168 ms and wrote the live NES cache. Frontend component
checksums passed all 198 entries; the deployed app-layer metadata passed all
4,263 entries before activation.

## Device gates

- [x] network fetch through a supported USB Wi-Fi dongle;
- [x] downloaded PNG paths resolve through the frontend library scanner;
- [ ] visually confirm the 107 resolved NES images in the graphic ROM list;
- [ ] confirm START/Apps/Scraping progress, cancel, result, and FE return after
  the repaired runtime;
- [ ] confirm downloaded artwork persistence after reboot.
