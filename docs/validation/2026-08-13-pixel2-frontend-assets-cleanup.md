# Pixel2 frontend assets and storage cleanup

## Implemented

- `plumos-sdcard-cleanup` targets `/mnt/plumos-user/roms` and
  `/mnt/plumos-user/images` by default;
- cleanup removes only `.DS_Store`, `._*`, `Thumbs.db`, `desktop.ini`, and
  `__MACOSX`, with lock, minimum interval, dry-run, and scan-cache invalidation;
- Chinese, Portuguese, French, and German catalogs were adapted to Pixel2;
- all six frontend catalogs contain the same 364 keys;
- Arduboy, Mega Duck, PuzzleScript, and Super Bros War receive deterministic
  190x156 Pixel2 theme badges.

## Host evidence

- `tests/test-app-layer-scripts.sh`: sidecar dry-run/delete fixture and all four
  generated PNG dimensions pass;
- `tests/test-implementation-audit.sh`: pass;
- `docker-build.sh frontend`: pass;
- `docker-build.sh app-layer --strict`: pass;
- implementation audit release blockers: 14 before this unit, 5 after it;
- remaining blockers: ADB authentication, thumbnail scraper, System Update,
  standalone PCSX-ReARMed, and standalone YabaSanshiro.

## Device gates

- run cleanup dry-run against the live empty Pixel2 user volume;
- switch through all six languages and inspect glyphs, clipping, and wrapping;
- inspect all four badges on the physical LCD.
