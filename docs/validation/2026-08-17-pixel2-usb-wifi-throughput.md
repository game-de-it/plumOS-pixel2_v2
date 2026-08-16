# Pixel2 USB Wi-Fi throughput investigation

Date: 2026-08-17

## Symptom and scope

The validated `0bda:8179` USB adapter associates, receives an IPv4 address,
and passes SSH/SFTP/FTP/Samba integrity checks, but a larger SFTP transfer is
unusably slow. Pixel2 has one USB port, so USB ADB and the Wi-Fi adapter cannot
be connected together. Driver changes were prepared over ADB, the port was
swapped to Wi-Fi for each measurement, and it was swapped back without a
reboot to collect kernel evidence.

The test payload was the same 30 MiB `libicudata.so.72.1` file for every SFTP
run. Each run had a 60-second limit and wrote to a temporary file on
`PLUMOS_USER`; all temporary files were removed afterwards.

## Excluded bottlenecks

- Local `PLUMOS_USER` write: 32 MiB in 1.38 seconds, about 23 MiB/s.
- FTP upload: about 80 KiB/s, so SFTP encryption and Dropbear are not the
  cause.
- USB link: high-speed 480 Mbps, runtime PM active and autosuspend disabled.
- CPU: about 1.0 GHz with the `ondemand` governor; it was not saturated.
- Reported radio link: 72.2 Mbps normally and 54 Mbps in forced 802.11g mode.
- Signal quality: 84--100/100 during the investigation.

The stock 5.10.198 kernel provides only the staging `r8188eu` module for this
adapter (`v4.1.4_6773.20130222`). It has no usable nl80211 power-save control,
so the shared plumOS `iw ... set power_save off` path used by MF cannot control
this driver. The module parameters are copied into the adapter registry during
initialization; a valid comparison therefore required unloading and reloading
the module rather than changing sysfs values after association.

## Controlled comparisons

| Driver load | Result after 60 seconds |
| --- | --- |
| stock defaults | 3,655,680 bytes, about 59 KiB/s |
| power/IPS/Smart PS disabled | 2,872,320 bytes, no improvement |
| HT, A-MPDU, 40 MHz and RX STBC disabled | SSH/SFTP setup itself timed out |
| `rtw_wifi_spec=1`, power/IPS/Smart PS disabled | 3,655,680 bytes, about 59 KiB/s |

The final compatibility-mode run reported strong signal and no kernel RX/TX
error counters. It recorded 3,737 RX packets, 55 RX drops, 2,035 TX packets,
zero TX drops, and zero RX/TX errors.

With `debug=9`, USB completion and AES receive processing remained clean while
the transfer was active. Accepted unicast data arrived only around once per
second, consistent with link-layer loss and TCP retransmission/backoff. USB
errors `-71`, `-104`, and `-19` appeared only when the operator physically
removed the Wi-Fi adapter and are not transfer failures. The driver's generic
`validate_recv_frame fail` messages were recurring beacon-management handling,
not evidence that payload frames failed decryption.

## Conclusion and next acceptance gate

SFTP, storage, CPU, USB autosuspend, driver power saving, HT/A-MPDU, and
`rtw_wifi_spec` are not the bottleneck. The remaining boundary is the old stock
`r8188eu` radio path interacting with this physical adapter and access point.
No diagnostic module option was made persistent.

Do not continue random driver tuning. The next controlled device test is one
of:

1. use the same adapter with a different 2.4 GHz access point configured for
   pure WPA2-PSK/AES; or
2. use a second known-good `0bda:8179` adapter with the current access point.

This separates access-point interoperability from a defective or marginal
adapter. If both adapters reproduce the failure across access points, a newer
kernel-compatible 8188EU driver must be sourced and built against the exact
stock 5.10.198 ABI before USB Wi-Fi bulk-transfer performance can be accepted.
