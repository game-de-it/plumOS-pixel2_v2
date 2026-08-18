# Pixel2 synchronous FunctionFS ADB

## Failure evidence

After Runtime `0.1.0-dev-21fba08` was installed, the frontend still reported
ADB `waiting`, and a physical cable replug did not recover it. macOS showed the
complete USB gadget rather than a missing cable or ownership conflict:

- product `plumOS Pixel2 ADB`, `18d1:4ee7`;
- ADB interface class/subclass/protocol `255/66/1`;
- one bulk-IN and one bulk-OUT endpoint;
- no entry in `adb devices -l` with either host ADB 35.0.2 or 36.0.2.

The macOS ADB server log rejected bulk endpoint `0x81` with
`failed to clear halt ... LIBUSB_ERROR_OTHER`. Persistent Pixel2 logs also
contain repeated `FUNCTIONFS_DISABLE` followed by reads failing with
`Cannot send after transport endpoint shutdown`.

## Missing family implementation

The established plumOS handheld adbd build forces legacy FunctionFS and
synchronous endpoint I/O because the stock kernels do not provide a reliable
Android property/AIO environment. Pixel2 had only enabled the FunctionFS daemon
path and still selected the default nonblocking AIO transport. That was an
unnecessary Pixel2-specific divergence.

`scripts/build-adbd-overlay.sh` now patches the upstream daemon to compile with:

```text
PLUMOS_ADBD_USB_FFS
PLUMOS_ADBD_LEGACY_FFS
PLUMOS_ADBD_SYNC_FFS
```

The installed source manifest identifies the result as
`legacy synchronous FunctionFS for Pixel2 stock kernel`. No foreign device
identity is shipped in the Pixel2 System.

## Host verification

- AArch64 adbd build: pass;
- generated binary SHA-256:
  `be773e29a210f67cda640baef7ea84bc93f573a7f26e3eddfd86c14d72eb2fec`;
- Pixel2 ADB policy/recovery fixture: pass;
- complete System rootfs fixture: pass;
- signed updater inspect with installed source `21fba08`: pass.

Signed System package:

```text
version=0.1.0-dev-5246728
source_version=0.1.0-dev-21fba08
package_sha256=164f9c6e49a2077c25a7d780b547be2a2e2718714a72fce74031e73df0d25081
system_sha256=99ce6ed925d7a510b0feff966ff470613827f8677a5ff58a8f72b02f33e64592
```

The package is retained under
`output/live/2026-08-19-pixel2-adbd-sync-ffs/`.

## SD staging

The attached 62.5 GB Pixel2 card was identified as `/dev/disk4`. Before any
write, active System A was confirmed as `21fba08` with SHA-256
`57b3e98d534bddb525c8cf67189a37f7f8c6cf7432abff0f273d08b76515167b`;
inactive System B read back at
`2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.

The signed `21fba08 -> 5246728` System package and its checksum sidecar were
added to the FAT32 update inbox. Card readback matched package SHA-256
`164f9c6e49a2077c25a7d780b547be2a2e2718714a72fce74031e73df0d25081`,
and the real updater accepted its signature, exact source version, device,
architecture, ABIs, manifest, and payload. Both existing System slots and the
zero-byte ADB recovery marker remained byte-identical.

## Offline recovery deployment

The subsequent read-only Runtime capture proved that the frontend had not
created a System update request. The only entry in
`frontend-system-update.log` was the earlier Runtime `8a98e3e -> 21fba08`
request, while `system-update-boot.log` reported `no-pending-state` and active
System A still identified itself as `21fba08`. The observed ADB `waiting`
result therefore did not test the synchronous FunctionFS binary.

To avoid another unobservable update attempt while ADB was unavailable, the
attached card was recovered offline. The five managed System A files were
first copied to
`/offline-backup-system-a-21fba08-pre-sync-ffs/` on PLUMOS_USER. The signed
package was verified before writing, copied through temporary slot filenames,
read back, and then installed as System A. Final verification showed:

- active System A version `0.1.0-dev-5246728`;
- System A SHA-256
  `99ce6ed925d7a510b0feff966ff470613827f8677a5ff58a8f72b02f33e64592`;
- manifest payload SHA-256 equal to the read-back SquashFS;
- Ed25519 signature verification successful;
- inactive System B unchanged at
  `2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.

ROM, BIOS, saves, settings, Runtime, and the update inbox were not changed.
Cold-boot ADB shell and repeated physical replug remain pending physical
acceptance.

## Physical result

System `5246728` still reported ADB `waiting` after a cold boot. The legacy
synchronous FunctionFS change is therefore rejected for Pixel2 and must not be
treated as a working fix. The subsequent rollback restores the physically
accepted `45b4505` nonblocking FunctionFS and single-replug contract while
preserving all unrelated later Pixel2 features.
