#!/bin/sh
# Sourced by the retained stock Pixel2 initramfs immediately after /flash is
# mounted.  This is a diagnostic hook only; it must not start stock userspace.
echo 'plumos-stock-initramfs=post-flash flash-mounted=1' >/dev/kmsg 2>/dev/null || true
echo 'plumos-stock-initramfs=post-flash flash-mounted=1' >/dev/console 2>/dev/null || true
