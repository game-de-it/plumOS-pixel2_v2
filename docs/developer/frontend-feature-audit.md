# Pixel2 frontend feature contract

## Scope

Pixel2のFEは、MFの共有frontend surfaceを基準にする。ハードウェアに存在しない
lid switch、複数audio output、内蔵Wi-Fiは表示だけをコピーせず、Pixel2の実在する
backendへ置き換える。一方、共有Appsやfile transfer daemonが未実装であることを理由
に、項目だけを隠してrelease blockerを回避してはならない。

## 2026-08-14 root cause

欠落は性能やstock kernelの制約ではなく、初期bring-up時の暫定実装がそのままrelease
判定へ入ったことが原因だった。

- ADBを明示opt-inへ変更したcommitにより、内蔵Wi-Fiのないfresh imageでも保守経路が
  無い状態を作れた。
- FEが書く`/mnt/plumos/config/network/services.conf`とSystem ADB serviceが読む
  `/state/plumos/config/network/services.conf`が分岐し、UI設定とboot policyが不一致だった。
- Pixel2 controllerはFTP/SFTP/Sambaを意図的に非表示にし、backendも
  `not_installed`を返していた。
- Apps catalogはScrapingとPyxel Setupだけを公開し、共有Appsの欠落はP1として
  release gateを通過できた。

## Required surface

機械可読な正本は`package/frontend-pixel2/feature-contract.json`である。

- START: UI Settings、System Settings、Network Settings、Apps、HELP、POWER
- POWER: 共通のSleep、Reboot、Shutdown、Cancel menuを開く。STARTには個別の
  Reboot/Shutdownを重複表示しない。
- Apps: Scraping、File Manager、Music Player、RetroArch、Pyxel Setup、PortMaster、
  Update PortMaster
- Ports system: `roms/ports/*.sh`からPixel2 PortMaster launcherへ接続
- NW Service: SSH、FTP、SFTP、Samba
- Network Settings: USB Wi-Fi接続、SSH/FTP/SFTP/Samba、接続情報を提供する。
- Settings: 共有surfaceとPixel2固有hardware capabilityを合わせた86 ID。

`scripts/audit-pixel2-implementation.py --release-gate`はcatalog、settings、action
handler、4 network services、11 mandatory componentsを検査する。さらに
`scripts/verify-app-layer.sh`はvisible Appの`$PLUMOS_ROOT` launcherが実在し実行可能で
あることと、各component checksumを検査する。未実装項目を非表示にしてreleaseする
運用へは戻さない。

## Persistence and boot contract

network service設定は`/mnt/plumos/config/network/services.conf`へ統一する。
`35-network-services`が起動時に`start-enabled`を実行する。設定未作成時はSSHを
ONにする。Pixel2の単一USB portはUSB Wi-Fiを優先するが、dongle非接続時はstock
OTGへ解放して起動中充電を許可する。FEにUSB ModeやADB項目は表示しない。
旧`usb_mode`、`adb_enabled`、FAT recovery markerは起動時に
削除するが、SSID/PSK、service設定、ROM、BIOS、saveは変更しない。

## Hardware acceptance still required

host gateは表示・導線・payload・永続化の欠落を検出するが、LCD、物理button、USB
enumeration、USB Wi-Fi dongle、LAN経由FTP/SFTP/Samba、各Appsの終了後FE復帰は実機
acceptanceで確認する。
