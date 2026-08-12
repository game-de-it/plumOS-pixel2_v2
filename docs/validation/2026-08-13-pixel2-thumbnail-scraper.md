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

## Device gates

- network fetch through a supported USB Wi-Fi dongle;
- START/Apps/Scraping navigation, progress, cancel, result, and FE return;
- downloaded artwork rendering and persistence after reboot.
