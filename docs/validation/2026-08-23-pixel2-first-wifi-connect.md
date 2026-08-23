# Pixel2 first Wi-Fi connection

Date: 2026-08-23

## Symptom

Two separately initialized release-candidate SD cards showed the same
behavior with the UGREEN AC650 / RTL8811CU adapter (`0bda:1a2b` switching to
`0bda:c811`): the first FE password submission reported that no IP address was
obtained, while submitting the same SSID and password a second time connected
successfully. Reproduction on the second SD rules out an isolated SD fault and
makes a password-entry mistake an insufficient explanation.

## Runtime evidence

The persistent device log on the second SD recorded this sequence:

| event | device uptime |
|---|---:|
| driver disk `0bda:1a2b` appeared | 76.88 s |
| mode switch completed as `0bda:c811` | 79.79 s |
| `8821cu` loaded | 80.39 s |
| first scan started | 82.15 s |
| first scan ended with 0 SSIDs | about 90 s |
| second scan started | 94.15 s |
| second scan returned 9 SSIDs | about 96 s |
| first connect began | 119.16 s |
| first WPA process started | 121.79 s |
| fixed 15-second WPA wait expired | 137.05 s |
| second connect began | 223.59 s |
| second WPA process started | 227.46 s |
| WPA, DHCP and gateway check succeeded | 242.03 s |

The first path failed at `stage=wpa_completed`; DHCP was never requested. The
second path completed WPA, DHCP and gateway validation in 14.57 seconds after
WPA startup. The active connection then reported RSSI -44 dBm, 434 Mbps link,
5 GHz channel 5240 MHz, USB autosuspend disabled, and no driver indication of
a DHCP or radio failure.

## Root cause

The manual FE connect path reused `WPA_WAIT_SECONDS=15`, which was designed to
keep automatic boot recovery bounded. A cold Realtek scan and association can
cross that boundary by a few seconds.

There was also a cleanup defect on a fresh card. Failure recovery looked for a
previous saved config before stopping the newly started candidate. With no
previous config, the failed candidate stayed alive after the FE reported
failure. It could continue scanning and associating, but the control script no
longer requested DHCP. Its continued activity warmed the driver and BSS cache,
so the second submission was more likely to complete inside 15 seconds.

This explains both observations: the first attempt had no IP address, and the
same credentials worked on the second attempt.

## Fix

Commit `dccd872` makes two bounded changes:

- automatic boot recovery keeps its existing 15-second WPA bound so boot time
  does not regress;
- an explicit FE `--connect-file` request gets a separate 30-second bound;
- every failed uncommitted candidate is stopped and its IP state cleared even
  when no previous saved configuration exists.

The saved Wi-Fi configuration is still committed only after WPA and DHCP both
succeed. A failed password therefore does not replace a known-good config.

## Verification

Host verification passed:

```text
./tests/test-pixel2-network-control.sh
pixel2_network_control=result-ok

./tests/test-pixel2-power-menu-sleep.sh
pixel2_power_menu_sleep=result-ok

./tests/test-app-layer-scripts.sh
app_layer_scripts=result-ok
```

The network fixture proves both the longer explicit-connect bound and cleanup
of a fresh-card failed candidate. Full `frontend` and strict `app-layer` builds
also passed as version `0.1.0-rc1-dccd872`.

The live deployment updated the network-control script together with
`VERSION`, root `manifest.json` / `checksums.sha256`, and the frontend component
manifest/checksum. The previous six files were backed up under
`/mnt/plumos-user/updates/backup-pre-wifi-first-connect-dccd872`. Device-side
file hashes matched the host build, the frontend component checksum passed,
and `plumos-system-update verify-runtime` returned `runtime_verify=result-ok`.

A physical explicit-connect test then completed in 18 seconds:

```text
result=connected
ip=192.168.10.107
gateway_ping=ok
elapsed=18
```

The saved config SHA-256 remained
`4d813f945cc9c5e76d6ba0741d05735a2d26850a4b5587199e5f7d2afc4e79e0`
before and after the test. An 18-second connection would have failed under the
old 15-second limit and therefore directly exercises the corrected boundary.

The original `0.1.0-rc1-28aaf65` image does not include this fix. The current
live SD does; the next release-candidate image must be regenerated from
`dccd872` or later.
