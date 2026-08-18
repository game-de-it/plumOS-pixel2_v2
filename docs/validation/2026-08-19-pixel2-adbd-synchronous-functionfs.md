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
`output/live/2026-08-19-pixel2-adbd-sync-ffs/`. Offline deployment, System A/B
promotion, a real ADB shell, and repeated physical replug remain pending.
