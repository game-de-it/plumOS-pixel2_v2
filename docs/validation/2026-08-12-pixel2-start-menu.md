# Pixel2 FE Start Menu Validation

Date: 2026-08-12

## Goal

Check that the Pixel2 frontend START menu matches the established plumOS MF and
V90S top-level menu contract, and verify that each entry reaches a working
frontend screen or power action path.

## MF/V90S reference

The MF and V90S frontend packages both expose the same START menu entries:

1. UI Settings
2. System Settings
3. Network Settings
4. Apps
5. HELP
6. Reboot
7. Shutdown

Both also expose a populated Apps submenu. Pixel2 intentionally keeps
`config/frontend/apps.json` empty until those app components are ported, so the
Apps entry opens a valid empty submenu instead of advertising unavailable app
binaries.

## Finding

Pixel2 previously exposed:

1. UI Settings
2. System Information
3. Network Information
4. HELP
5. Reboot
6. Shutdown

The missing `System Settings`, `Network Settings`, and `Apps` entries made the
Pixel2 START menu diverge from MF/V90S.

## Change

Commit:

```text
c875dd8 fix: align Pixel2 frontend start menu
```

Changes:

- `package/frontend-pixel2/menus.json` now matches the MF/V90S top-level START
  menu sequence.
- Added Pixel2 app-layer helpers required by those screens:
  - `bin/plumos-display-control`
  - `bin/plumos-volume-control`
  - `bin/plumos-network-control`
  - `bin/plumos-network-services`
- Updated app-layer verification so these helpers and menu actions are required.

## Host validation

```sh
./tests/test-app-layer-scripts.sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh app-layer --strict
```

Results:

```text
app_layer_scripts=result-ok
frontend_component=result-ok output=/work/output/frontend/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Generated app-layer manifest:

```text
source_ref=c875dd8
```

Key output hashes:

```text
c6bf2e13ced7592e998f212db261c508c42c35148fa52535b70a44dcd5fcac25  config/frontend/menus.json
3b84635d6d9b1d86262eb51e158223cabb95a223883ebe3e9839a05f8905599e  checksums.sha256
1217ccf1be3f82657ec19c1db2b5af00c97b3175bd2663b0e047dd97abc21763  bin/plumos-display-control
73145d546327d0e2f16d3d720a32e8e1ac283373015a3b65e0c548cb3cc68ff5  bin/plumos-volume-control
deeeac479c3a2317af63c7de86f59d152e7a41652af9ddbee08dcb2efafda660  bin/plumos-network-control
0dd9d1dbd61e7be206bfc989f7bea1449d61c3b47edc9d694d127684037581cc  bin/plumos-network-services
```

## Live deployment

The generated app-layer managed files were staged through ADB, verified with
`sha256sum -c checksums.sha256`, extracted into `/mnt/plumos`, and verified
again in place.

Live checks:

```text
source_ref=c875dd8
c6bf2e13ced7592e998f212db261c508c42c35148fa52535b70a44dcd5fcac25  config/frontend/menus.json
3b84635d6d9b1d86262eb51e158223cabb95a223883ebe3e9839a05f8905599e  checksums.sha256
```

Helper status:

```text
display_status
backend=pixel2-pwm-backlight
brightness=10
backlight_raw=200
max_brightness=255

volume_status
backend=pixel2-state-only
volume=8
alsa_card=rockchiprk817

network_status
state=NO_WIFI_INTERFACE

adb_service_status
service=adb
state=running
summary=ADB over USB FunctionFS
enabled=1

ssh_service_status
service=ssh
state=stopped
summary=SSH stopped
enabled=1

ftp_service_status
service=ftp
state=not_installed
summary=FTP not packaged for Pixel2 yet
enabled=0
```

Volume is currently a Pixel2 state-only helper because no mixer command backend
is packaged in the Pixel2 rootfs/app-layer yet. The RK817 ALSA card is visible,
and actual mixer routing should be completed with the audio router work.

## START entry functional checks

The frontend was restarted after deployment and loaded `/mnt/plumos`:

```text
frontend_restart=ok
PLUMOS_ROOT=/mnt/plumos
PLUMOS_INPUT_AB_LAYOUT=east-confirm
PLUMOS_INPUT_A_CODE=305
PLUMOS_INPUT_B_CODE=304
```

`plumos-text-ui menu start` confirmed the live menu:

```text
1. UI Settings       internal   internal:ui-settings
2. System Settings   internal   internal:system-settings
3. Network Settings  internal   internal:network-settings
4. Apps              submenu    menu:apps
5. HELP              internal   internal:help
6. Reboot            system     system:reboot       confirm=yes
7. Shutdown          system     system:shutdown     confirm=yes
```

Frontend script checks:

| Entry | Result |
| --- | --- |
| UI Settings | opened `UI Settings`, 11 entries |
| System Settings | opened `System Settings`, 12 entries |
| Network Settings | opened `Network Settings`, 4 entries |
| Apps | opened `Apps`, 0 entries |
| HELP | opened `HELP`, 9 entries |
| Reboot | opened power confirm screen on Reboot |
| Shutdown | opened power confirm screen on Shutdown |

## Power action checks

Reboot was executed through the FE action path using the same environment as the
booted frontend (`PLUMOS_BUSYBOX=/bin/busybox`). The device rebooted and ADB
returned; `uptime` reset to 0 minutes and the frontend PID changed.

```text
power_action=reboot
power_action_title=Restarting
power=prepare-start action=reboot
power=stop-process name=frontend pid=1426 signal=TERM
power=stop-process name=frontend pid=1426 signal=KILL
power=unmount-result target=/roms rc=0
power=unmount-result target=/mnt/plumos-user rc=0
power=prepare-end action=reboot
```

Shutdown was checked through the FE action path with
`PLUMOS_CONTROLLER_POWER_DRY_RUN=1` to avoid powering off the active test unit:

```text
power_action=shutdown
power_action_title=Shutting Down
status: shutdown complete poweroff
power=result-requested device=pixel2 action=shutdown dry_run=1
```

Actual FE-menu shutdown remains a physical-device terminal test because it
powers off the unit and drops ADB. The underlying RK817 shutdown path was
validated separately in the Pixel2 power-management record.

## 2026-08-23 POWER entry consolidation

The original START catalog duplicated Reboot and Shutdown even though both
entries only opened the same four-choice power menu with a different initial
cursor. The standard Pixel2 START surface now contains six entries:

```text
1. UI Settings       internal   internal:ui-settings
2. System Settings   internal   internal:system-settings
3. Network Settings  internal   internal:network-settings
4. Apps              submenu    menu:apps
5. HELP              internal   internal:help
6. POWER             system     system:power
```

`system:power` opens the existing Sleep/Reboot/Shutdown/Cancel menu without
executing or preselecting a terminal action. The legacy `system:reboot` and
`system:shutdown` handlers remain for compatibility with an older catalog,
but neither action nor entry is present in the generated standard menu. The
feature contract and host fixture require the exact six-entry catalog and the
new handler.

The signed Runtime package was then deployed to the cold-booted UGREEN AC650
unit over RTL8821CU Wi-Fi at `192.168.10.107`:

```text
previous Runtime: 0.1.0-dev-875227e
installed Runtime: 0.1.0-dev-e3b44c5
package sha256: f6ca135d80a7ad42193fb65270613678829907156b0879b87c1c2357c3b52404
update result: runtime_healthy
Frontend component checksum: OK
Runtime checksum: runtime_verify=result-ok
```

The installed `menus.json` and `plumos-text-ui menu start` both reported the
same six-entry catalog. The installed production frontend was also exercised
in its non-rendering text-script path using
`start,down,down,down,down,down,a,q`. It selected entry 6, opened the shared
POWER screen, exposed Sleep/Reboot/Shutdown/Cancel, and left Cancel selected by
default. No terminal power action was executed during this route test.
