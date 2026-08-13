# plumOS Pixel2 boot splash

`bootlogo.png` is the editable, logical 640x480 plumOS artwork shared with the
other plumOS handheld ports. `oemsplash-1080.png` is the Pixel2 boot-ready
480x640 RGB PNG.

The Pixel2 panel is exposed to the retained stock initramfs in portrait memory
orientation. The boot-ready image is therefore rotated 90 degrees
counter-clockwise so that it appears as 640x480 landscape on the physical
screen:

```sh
magick bootlogo.png -background black -alpha remove -alpha off \
  -colorspace sRGB -type TrueColor -rotate -90 -strip \
  PNG24:oemsplash-1080.png
```

The retained stock initramfs checks `/flash/oemsplash-1080.png` before its
embedded `/splash/splash-1080.png`. The SD image builder installs this file at
the root of `PLUMOS_BOOT`; no kernel or initramfs modification is required.

Validate the boot-ready file with:

```sh
python3 scripts/verify-pixel2-boot-splash.py \
  package/boot-assets-pixel2/oemsplash-1080.png
```
