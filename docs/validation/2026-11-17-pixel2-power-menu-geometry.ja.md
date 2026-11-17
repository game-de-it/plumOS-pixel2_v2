# Pixel2 電源メニュー復帰時のDRM表示位置

[English](2026-11-17-pixel2-power-menu-geometry.md)

実機107、v0.1.4上のGambatte／ゼルダDX／RGUIで再現を確認した。
ゲームplane 81の表示領域は電源メニュー前が`0,54,480,533`、Bで戻った後は
`0,0,480,533`、RetroArchメニュー開閉後は元の値へ戻った。
Pixel2は縦scanoutを回転表示するため、この差が横方向のずれになる。

原因はhardware-key serviceのDRM handoffで、旧`drmModeGetPlane()`の座標が
実際のatomic propertyに反して0を返すこと。幅・高さもframebufferサイズで代用していた。
`9dd4d9c`ではCRTC/SRCの8 propertyを読み、拡大縮小・切り出し・位置を保持して
SIGCONT前に復元する。取得に失敗したplaneは無効化しない。

AArch64 build、負の座標・小数source座標・不完全propertyなどのC回帰試験、既存の
power/sleep試験に合格。実機へbinaryとchecksum・manifest群を整合させて限定反映し、
ゲームを維持したままhardware-key serviceだけを再起動した。

実際の電源メニュー→Bを3回繰り返し、両planeの全座標が前後で一致した。
RetroArch PIDは12019のまま、ALSA再生pointer進行、component／全Runtime checksum検証、
RetroArch設定hash保持にも合格した。実機metadataは限定patchとして記録し、
更新前fileは`/mnt/plumos/backups/power-geometry-1794925367`へ保持している。

修正後のLCD目視確認はoperatorへ依頼する。release公開とSD image再生成は行っていない。
