#!/bin/sh
# Sourced by the retained stock Pixel2 initramfs immediately after /flash/SYSTEM
# is loop-mounted at /sysroot.
echo 'plumos-stock-initramfs=post-sysroot system-mounted=1' >/dev/kmsg 2>/dev/null || true
echo 'plumos-stock-initramfs=post-sysroot system-mounted=1' >/dev/console 2>/dev/null || true
if [ -e /sysroot/usr/lib/systemd/systemd ]; then
    echo 'plumos-stock-initramfs=post-sysroot handoff-target=present' >/dev/kmsg 2>/dev/null || true
else
    echo 'plumos-stock-initramfs=post-sysroot handoff-target=missing' >/dev/kmsg 2>/dev/null || true
fi
