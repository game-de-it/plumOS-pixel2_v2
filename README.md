# plumOS Pixel2

[日本語](README.ja.md) | [User Guide](docs/user/README.md) | [Developer Guide](docs/developer/README.md)

plumOS Pixel2 is a Linux distribution for the RK3326S-based GKD Pixel2. It
keeps the hardware-compatible stock boot substrate and starts a plumOS-owned
system, frontend, services, and emulator environment after the stock initramfs
hands off to `SYSTEM`.

## Main features

- six-tile game frontend with text, graphic, and gallery views;
- RetroArch, PicoArch, standalone emulators, PICO-8, Pyxel, and PortMaster;
- FAT32 `PLUMOS_USER` volume for ROMs, BIOS files, images, saves, and updates;
- global volume, brightness, power menu, sleep, restart, and shutdown controls;
- supported USB Wi-Fi adapters with SSH, SFTP, FTP, and Samba services;
- signed Runtime updates and A/B System updates with rollback checks.

ROMs and user-supplied BIOS files are not included. Use only content that you
are legally entitled to use.

## Important Pixel2 limitations

- This image is for the GKD Pixel2 only.
- Use a separate SD card of at least 16 GB. Do not overwrite the original
  stockOS card.
- First boot expands the System partition, creates `PLUMOS_USER`, and may
  restart once automatically. Do not remove power during setup.
- Pixel2 has one USB port. A Wi-Fi adapter and a charger cannot be connected at
  the same time without external hardware. Removing Wi-Fi releases the port so
  the running device can charge.
- ADB is not included. Remote maintenance uses Wi-Fi and SSH/SFTP.
- Always use the POWER menu to shut down before removing the SD card.

## Documentation

For installation, controls, storage, networking, emulators, updates, and
troubleshooting, start with the [User Guide](docs/user/README.md).

For builds, boot ownership, Runtime and System layout, app-layer deployment,
hardware integration, and validation, use the
[Developer Guide](docs/developer/README.md).

The complete bilingual documentation index is under
[`docs/`](docs/README.md). Date-stamped files under `docs/validation/` are
engineering evidence and may describe superseded experiments; they are not
end-user instructions.

## Project status

The Pixel2 port is undergoing release-candidate validation. A successful host
build or checksum does not by itself prove display orientation, controls,
audio, sleep, charging, storage, or emulator behavior on physical hardware.
Current implementation and release blockers are tracked in
[TODO](TODO.md) and the
[implementation inventory](docs/developer/implementation-status.md).

## License and Notices

plumOS-authored material is [MIT licensed](LICENSE). Stock/vendor boot material
and bundled third-party components retain their own terms; see
[NOTICE](NOTICE.md) and [Third-Party Notices](THIRD_PARTY_NOTICES.md).
