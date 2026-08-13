#!/bin/sh
# Sourced by the retained stock Pixel2 initramfs immediately after /flash is
# mounted. The stock init calls load_splash before mount_flash, so its normal
# /flash/oemsplash lookup cannot see the boot volume. Redraw the plumOS image
# here while the initramfs framebuffer helper is still available.
echo 'plumos-stock-initramfs=post-flash flash-mounted=1' >/dev/kmsg 2>/dev/null || true
echo 'plumos-stock-initramfs=post-flash flash-mounted=1' >/dev/console 2>/dev/null || true

if [ -x /usr/bin/ply-image ] && [ -f /flash/oemsplash-1080.png ]; then
    if /usr/bin/ply-image /flash/oemsplash-1080.png >/dev/null 2>&1; then
        echo 'plumos-stock-initramfs=boot-splash result=plumos' >/dev/kmsg 2>/dev/null || true
    else
        echo 'plumos-stock-initramfs=boot-splash result=draw-failed' >/dev/kmsg 2>/dev/null || true
    fi
else
    echo 'plumos-stock-initramfs=boot-splash result=asset-or-renderer-missing' >/dev/kmsg 2>/dev/null || true
fi
