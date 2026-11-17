# Pixel2 整数スケールの座標系統一

[English](2026-11-18-pixel2-integer-source-coordinates.md)

修正commitは`62b76ce`、patch 038。前回のRGUI修正で維持していた整数倍率の
計算方針を更新する。

QuickNES／悪魔城伝説（256x224）で、整数ON時にRGUIは334x256、Ozoneは512x392と
なっていた。論理画面寸法・回転・aspectを異なる座標系で共通helperへ渡していたため、
Axis Yがゲーム本来の縦軸を指していなかった。

共通helperに明示的なsource回転を渡せる入口を追加。従来入口の自動回転は維持し、
Pixel2 DRMはsource座標で整数計算した後に既存のsoftware回転を適用する。
GLESもsource座標で整数計算してから、viewport矩形を物理縦画面へ回転する。
非整数fallbackは引数のaspectを使い、別座標系のglobal aspectを読み直さない。
グラフィカルmenu描画後にはゲーム用viewportを復元する。

AArch64 build、実修正関数を使う回帰試験、既存RGUI試験、patch逆適用検査に合格。
実機はbinaryとchecksum・manifestを一括で限定反映した。

初期の自動比較は有効なgame frameへ到達せず合格証拠とはしない。描画安定後の
DRM property/captureで、RGUIのphysical planeが`16,27,448,586`、すなわち
logical `586x448`であること、menu開閉後も同じことを確認した。RGUI menuは全面表示。
Ozoneはprimary planeのcaptureで同じ大きさの中央表示となり、ON/OFFとmenu復帰を確認。
plain DRMはoverlay、GLESはprimaryを使うため、primaryだけの黒画像でRGUI失敗とは判定しない。

Axis Yでは縦224が2倍の448となる。Core Providedの非正方形pixel比補正により横は586に
なるため、横も元pixelの整数倍にしたい場合はAxis Y/Xを使う。

一時設定による比較中の保存済みcfg hashは
`627eb7842a4a3fdceb9fb1f273429c988791de8a4fd84e860e42cbfd2a80631b`で不変。
検証process終了後にFEを復帰。最終LCD目視はoperator確認待ち。
release公開・SD image生成は行っていない。

## 起動中PCEの実機確認

operatorがFEから起動した`AirZonk_U.pce`／`mednafen_pce_fast`を、Ozone/GLES、
整数ON・Axis Y・Underscale・Core Providedで確認した。coreの初期geometryは
256x243だが、実際のgame中は256x240、aspect 1.215へ動的変更されている。
menu開閉前後のscanout captureはどちらも非黒領域が`(28,0)-(612,480)`、すなわち
584x480の中央表示だった。縦240→480の2倍で整数スケールが適用されている。
横584はcore指定のpixel比補正を含む。

RetroArch PID 19377を維持し、ALSA RUNNING、1秒でhw_ptrが96984→145176へ進行。
保存cfg hashは`b08b271c864a90ad21a33302aaff30235853fcdd1ed30bf174c50e6807569dd2`で
不変。ゲームは起動したままとし、追加修正は行っていない。
証跡は`output/validation/2026-11-18-pce-integer/`。今回の256x240での合格であり、
PCE全タイトル・全解像度の検証済みという意味にはしない。
