# Install plumOS Pixel2

[日本語](install.ja.md)

## What you need

- a GKD Pixel2;
- a reliable SD card with at least 16 GB readable capacity;
- an SD card reader;
- the Pixel2 release image and its published SHA-256 checksum;
- Raspberry Pi Imager, balenaEtcher, or another raw-image writer.

Use a card separate from the original stockOS card. Writing an image erases the
selected card, so confirm the device and capacity before starting.

## Write and start the card

1. Verify that the downloaded image SHA-256 matches the published value.
2. Select the uncompressed Pixel2 `.img` in the image writer.
3. Select the new SD card and write the image.
4. Wait for the writer's verification to complete, then eject the card safely.
5. Insert the card into Pixel2 and power it on.
6. Leave the device powered while the first-boot setup runs. It expands
   `PLUMOS_SYS`, creates `PLUMOS_USER`, and may restart once automatically.
7. Setup is complete when the normal frontend appears and remains responsive.

Do not copy ROMs into the small boot volume before first boot. The host-readable
`PLUMOS_USER` volume does not exist until provisioning has completed.

## Add content after setup

1. Open `START -> POWER -> Shutdown` and wait for the screen to turn off.
2. Remove the SD card and connect it to the computer.
3. Open `PLUMOS_USER`.
4. Copy games into the appropriate folder under `roms/` and required BIOS files
   under `bios/`.
5. Eject the card safely, return it to Pixel2, and boot normally.

See [SD card and folders](storage.md) for the directory layout. Keep an original
copy of saves and important content on another disk.
