# plumOS Pixel2 Notice

plumOS Pixel2 uses a checksum-registered GKD Pixel2 stock boot substrate: the
Rockchip boot prefix, Linux 5.10.198 kernel image, runtime DTB, stock initramfs,
selected kernel modules, and firmware required to start the plumOS-owned
`SYSTEM` and app layer.

Those stock/vendor-derived files retain their original terms. They are not
covered by the plumOS MIT License. Their origin, hashes, and immutable boundary
are recorded by the stock capture manifests and
`package/licenses-pixel2/pixel2-stock-vendor-runtime-NOTICE.txt`.

plumOS-authored source, scripts, configuration, documentation, integration
patches, and artwork are licensed under the repository `LICENSE` unless a file
states otherwise. The MIT License does not relicense RetroArch, libretro cores,
standalone emulators, PortMaster, Python/Pyxel packages, fonts, libraries,
firmware, or vendor material.

Release images do not include ROMs, game BIOS files, saves, network
credentials, private signing keys, personal SSH keys, or user-installed
PortMaster/PICO-8/Pyxel content. Third-party details and explicitly documented
upstream-license gaps are recorded in `THIRD_PARTY_NOTICES.md` and
`THIRD_PARTY_NOTICES.ja.md`.
