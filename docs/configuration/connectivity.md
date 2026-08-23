# Pixel2 Connectivity

[日本語](connectivity.ja.md) | [User network guide](../user/network.md)

This is a developer compatibility reference. End users should follow the user
network guide instead.

Pixel2 has no built-in Wi-Fi. USB Wi-Fi plus SSH/SFTP is the maintenance path.
Release images do not ship ADB, FunctionFS, a USB Mode selector, or FAT recovery
markers. If no Wi-Fi credentials exist, the network remains inactive.

The single port is a Wi-Fi-priority dual-role OTG port. On cold boot the System
keeps stock OTG long enough to detect upstream VBUS. With no charger present it
runs one bounded host probe. Removing a dongle returns the controller to OTG so
a charger can be attached while the OS is running. USB Wi-Fi and USB charging
cannot be used simultaneously.

## USB Wi-Fi

The normal `wpa_supplicant.conf` is stored on the state volume:

```text
/plumos/config/wpa_supplicant.conf
```

Example:

```conf
ctrl_interface=/run/wpa_supplicant
update_config=0
country=JP

network={
    ssid="example"
    psk="replace-this"
}
```

Credentials must never be committed to Git or included in an image. At boot,
the runtime detects a USB WLAN interface and starts `wpa_supplicant` and
BusyBox `udhcpc`.

When saved Wi-Fi is enabled, `plumos-wifi-recovery` runs one BusyBox kernel
uevent monitor. It schedules a single bounded
`plumos-network-control --wifi on` after a three-second settle only when a
`wlan*` interface appears or a supported RTL8821CU USB identity is added.
Locks coalesce duplicate USB/net events. There is no periodic polling and no
unbounded retry. Disabling Wi-Fi validates and stops the monitor PID.

The System contains the stock `r8188eu` path plus a reproducibly built
`8821cu.ko` for RTL8811CU/RTL8821CU. The latter uses the stock Pixel2 5.10.198
ABI; the V90S 4.9 module binary is never reused.

Saved credentials do not grant permanent host ownership. Upstream VBUS wins.
Otherwise the service switches to host mode and waits for a downstream device,
then always returns to OTG if none appears. The Pixel2 extcon `USB-HOST` value
was not a reliable physical-presence signal, so it is not used as one.

Dongle removal releases OTG immediately, except while an RTL8821CU
`0bda:1a2b` storage identity is intentionally switching modes. That path gets a
five-second mode-switch interval followed by an eight-second downstream
absence check. Transition markers prevent a deliberate DWC2 reset from being
misclassified as physical removal.

After OTG release, an unpowered reinserted dongle cannot generate an event.
FE Wi-Fi ON, scan, and connect therefore request one bounded host probe when no
adapter is visible. Boot, FE, and recovery probes share one lock. A failed
probe returns to OTG and does not break charging standby.

UGREEN AC650 may first enumerate as `0bda:1a2b Realtek DISK`. Only for that ID,
the Wi-Fi path runs bounded `eject -s` on the associated `/dev/sr*`, waits for
`0bda:c811`, and loads `8821cu`. Adapters appearing directly as `0bda:c811` or
`0bda:c820` are also supported through module aliases. The driver build keeps
the V90S-proven feature contract: USB autosuspend disabled and driver power
saving enabled. Do not change power parameters without physical A/B throughput
measurements.

## Network Services

The service contract follows V90S/MF. SSH is enabled on fresh images and runs
Dropbear on TCP port 22:

```text
user: root
password: plumos
```

The factory password is public. Use it only on a trusted LAN and change it with
`plumos-ssh-password set` or disable SSH in Network Services. Only the salted
SHA-512 hash is kept at `/mnt/plumos/config/ssh/shadow`; updates do not replace
an existing password. SFTP uses the same account and port.

Public keys can be placed in `/root/.ssh/authorized_keys`. Host keys are
generated under `/mnt/plumos/config/ssh` on first boot. FTP is anonymous;
Samba exposes `SDCARD` with `plumos / plumos`. Service choices persist in
`/mnt/plumos/config/network/services.conf` and are restored on later boots.
