# Pixel2 Wi-Fi-only USB policy

Date: 2026-08-20

## Decision

Pixel2の単一USB portはWi-Fi優先のdual-role OTGとする。物理dongle接続中だけUSB
hostを使用し、抜去またはupstream charger検出後はstock OTGへ解放する。plumOS
Pixel2ではADBを提供しない。stock boot prefix、Image、kernel、initramfs、runtime
DTBは変更しない。

## Removed surface

- Systemのadbd、FunctionFS/configfs gadget、device-role強制helper;
- ADB watchdog/recovery、sleep resume連携、FAT recovery marker;
- Runtime network serviceのADB操作とUSB Mode API;
- FEのUSB Mode、ADB service、ADB status;
- adbd build source、host ADB helper、ADB smoke tooling。

## Migration and preservation

起動時に旧`usb_mode`、`adb_enabled`、`plumos-enable-adb`だけを削除する。SSID/PSK、
SSH/FTP/SFTP/Samba設定、ROM、BIOS、save、PortMaster、ユーザー設定は変更しない。
保存済みWi-Fi設定があり、extconが物理`USB-HOST=1`を報告する場合だけUSB host再列挙を
行う。bounded Wi-Fi recoveryのuevent monitorはdongle removeとcharger/extcon changeも
role再調停へ接続する。

## Host acceptance

- network-control、Wi-Fi uevent recovery、USB host re-enumeration;
- power/sleep regression;
- System source/rootfs gates;
- frontend feature contract and release audit;
- generated System/Runtime/SD image manifests contain the final source ref.

Cold bootでのRTL8821CU association、IP取得、SSH/SFTPは新規imageを実機へ書き込んだ
後のphysical acceptanceとする。
