# Pixel2 Wi-Fi-only USB policy

Date: 2026-08-20

## Decision

Pixel2の単一USB portはUSB Wi-Fi dongle専用とする。plumOS Pixel2ではADBを
提供しない。stock boot prefix、Image、kernel、initramfs、runtime DTBは変更しない。

## Removed surface

- Systemのadbd、FunctionFS/configfs gadget、device-role強制helper;
- ADB watchdog/recovery、sleep resume連携、FAT recovery marker;
- Runtime network serviceのADB操作とUSB Mode API;
- FEのUSB Mode、ADB service、ADB status;
- adbd build source、host ADB helper、ADB smoke tooling。

## Migration and preservation

起動時に旧`usb_mode`、`adb_enabled`、`plumos-enable-adb`だけを削除する。SSID/PSK、
SSH/FTP/SFTP/Samba設定、ROM、BIOS、save、PortMaster、ユーザー設定は変更しない。
保存済みWi-Fi設定がある場合はUSB host再列挙とbounded Wi-Fi recoveryを起動する。

## Host acceptance

- network-control、Wi-Fi uevent recovery、USB host re-enumeration;
- power/sleep regression;
- System source/rootfs gates;
- frontend feature contract and release audit;
- generated System/Runtime/SD image manifests contain the final source ref.

Cold bootでのRTL8821CU association、IP取得、SSH/SFTPは新規imageを実機へ書き込んだ
後のphysical acceptanceとする。
