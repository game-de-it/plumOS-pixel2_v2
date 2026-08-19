# Pixel2 System slot metadata transaction

Date: 2026-08-20

## Finding

The signed A/B updater correctly replaced the inactive SquashFS, `.sha256`,
signed `manifest.json`, and `manifest.sig`. The image-build text manifest named
`system-{a,b}.manifest` was left at its factory-seed version, source ref, size,
and hash. The dispatcher did not consume this text file, so boot and signature
integrity were unaffected, but offline capture could report misleading data.

## Fix

System packages now include the embedded System `source_ref` and
`source_date_epoch` in their signed package manifest. After mandatory SquashFS
readback, the updater atomically writes all slot metadata before it commits the
pending slot:

- `system-{slot}.sha256`
- `system-{slot}.manifest`
- `system-{slot}.manifest.json`
- `system-{slot}.manifest.sig`

Older packages remain accepted. Their text manifest falls back to the package
version as source ref and epoch zero, while the signed JSON remains canonical.

The update fixture verifies the inactive slot text version, source ref, size,
hash, signed JSON, and signature before health promotion.
