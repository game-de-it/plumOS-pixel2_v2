# アーキテクチャと所有範囲

[English](architecture.md)

## レイヤーモデル

```text
Pixel2 stock boot substrate
  Rockchip boot prefix / stock Image 5.10.198 / stock initramfs
  checksum登録済みstock runtime DTB
                       |
PLUMOS_BOOT上のplumOS SYSTEM
  /sbin/init / init.d / USB Wi-Fi / rootfs診断
                       |
PLUMOS_SYS上のplumOS app layer
  /mnt/plumos/bin / cores / config / factory-defaults / manifest
                       |
Pixel2 user/content volume
  /mnt/plumos-user/roms / bios / Images / Screenshots / updates
```

stock userlandは通常runtimeへ含めません。採用境界はstock initramfsから`SYSTEM`への
handoffで、`/sbin/init`以降のOS動作をplumOSが所有します。

## Process・mount所有範囲

| path・resource | 所有者 | 規則 |
| --- | --- | --- |
| `/boot` | stockがmountする`PLUMOS_BOOT` | 通常read-only。Image、DTB、SYSTEM、manifest |
| `/mnt/plumos` | `PLUMOS_SYS` | app-layer ABI、managed runtime、active config、log |
| `/mnt/plumos-user` | `PLUMOS_USER` | ROM、BIOS、media、画像、update inbox |
| `/roms` | user volumeからbind | FE・launcher共通ROM root |
| `/state` | `/mnt/plumos/state`からbind | early/runtime log互換path |
| `/run/plumos` | tmpfs | PID、lock、input/audio/display一時状態 |
| `/dev/fb0` | foreground owner 1つ | FE、emulator、app、power画面 |
| `/dev/input/event2` | Pixel2 joypad | FEとhardware-key。foreground重複を避ける |
| `gpio-keys` | hardware-key service | 音量とSELECT+音量の明るさ |
| RK817 PMIC | safe power helper | charge modeまたは`DEV_OFF` |

## 永続・mutable data

置換可能な管理対象はbinary、library、core、FE catalog、theme、factory default、NOTICE、
manifest、checksum、`SYSTEM`です。

実機所有のmutable dataはactive設定、ROM、BIOS、artwork、save/state、log、Wi-Fi認証、
SSH state、Pyxel project、PortMaster install gameなどのuser app stateです。updateと
live deployはこれらを保持しなければなりません。

## 現在の実装境界

実装済み:

- stock boot substrateとplumOS所有`SYSTEM`;
- FE、scanner、RetroArch、libretro catalog、PicoArch、OpenBOR、DraStic、PPSSPP、
  PICO-8、Pyxel、PortMaster、File Manager、Music Player;
- SSH/SFTP/FTP/Sambaを備えたUSB Wi-Fi保守経路;
- Pixel2 input map、global hardware key、power menu、sleep、充電mode、RK817 shutdown;
- RK817対応audio routing、署名Runtime transaction、A/B System update/rollback。

未完了はrepository license、release payload全体のthird-party notice監査、CI/publication
gate、残るBIOS inventoryとWi-Fi adapter matrix、最終release candidate実機acceptanceです。
現在値は`TODO.md`を正とします。
