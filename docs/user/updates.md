# System updates

[日本語](updates.ja.md)

Pixel2 uses signed update packages. Runtime updates change managed applications
and services on `PLUMOS_SYS`; System updates write only the inactive A/B System
slot on `PLUMOS_BOOT`.

## Install an update

1. Read the release notes and confirm that the package targets Pixel2.
2. Verify the downloaded package SHA-256 when one is published.
3. Copy the package into `PLUMOS_USER/updates` by card reader or a network
   transfer service.
4. Keep sufficient battery charge or use a stable power source before starting.
5. Open `START -> System Settings -> System Update`.
6. Confirm the package and wait for verification, installation, and any
   automatic restarts to complete.
7. Confirm that the normal frontend starts and the System Information page
   shows the expected version.

Never remove the SD card or power during an update screen. The update engine
checks the signature, device, version, ABI, payload hash, and final readback
before switching generations. An unconfirmed generation is not promoted and
the previous managed generation is retained for recovery.

## Data preservation

Managed updates do not intentionally replace ROMs, BIOS files, saves, states,
screenshots, Wi-Fi credentials, SSH host keys, PortMaster content, or active
user configuration. Back up important data anyway before a release upgrade.

Do not extract an update archive manually over `/mnt/plumos`; managed files and
their manifests/checksums must remain one atomic update unit.
