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

- START: UI Settings、System Settings、Network Settings、Apps、HELP、Reboot、Shutdown
- Apps: Scraping、File Manager、Music Player、RetroArch、Pyxel Setup、PortMaster、
  Update PortMaster
- NW Service: SSH、FTP、SFTP、Samba、ADB
- Settings: MFと共通の89 ID。Pixel2固有のhardware capability判定は保持する。

`scripts/audit-pixel2-implementation.py --release-gate`はcatalog、89 settings、action
handler、5 network services、11 mandatory componentsを検査する。さらに
`scripts/verify-app-layer.sh`はvisible Appの`$PLUMOS_ROOT` launcherが実在し実行可能で
あることと、各component checksumを検査する。未実装項目を非表示にしてreleaseする
運用へは戻さない。

## Persistence and boot contract

network service設定は`/mnt/plumos/config/network/services.conf`へ統一する。
`35-network-services`が起動時に`start-enabled`を実行する。ADBは設定未作成時だけON、
明示値を優先し、FAT32 rootの`plumos-enable-adb`をrecovery overrideとする。

## Hardware acceptance still required

host gateは表示・導線・payload・永続化の欠落を検出するが、LCD、物理button、USB
enumeration、USB Wi-Fi dongle、LAN経由FTP/SFTP/Samba、各Appsの終了後FE復帰は実機
acceptanceで確認する。
