# Pixel2 RGUIと整数スケール

[English](2026-11-17-pixel2-rgui-integer-scaling.md)

plain DRM経路ではviewportサイズのbufferへsoftware scalingするが、設定変更時に
planeだけを再設定し、core解像度が同じだと古いbuffer寸法を使い続けていた。
また、ゲームの整数スケールをRGUIにも適用していた。

patch 037では設定変更を記録し、RGUIがゲーム設定を復元した後の最初の実ゲームframeで
viewportを再計算する。変更があればbufferを再生成する。RGUIのlayer 2と初期viewportは
ゲームの整数スケールから独立させ、既存の画面4:3方針を維持する。

Fit Screen試験では、RGUIのaspect復元でmain surfaceが破棄された直後にNULLへ戻ろうと
して初期候補が終了（rc139）する経路も発見。最終版ではNULLを復元せず、次のgame frameで
再生成する。関連commitは`1bd48ec`、`ed79dbc`、`7f01f94`。

AArch64 build、実際の修正関数を使うC harness（`tests/test-pixel2-drm-integer.py`）、
patch逆適用検査に合格。実機107へbinaryと対応checksum・manifestを限定反映し、
前回の電源メニュー修正と既存設定は維持した。

QuickNES／悪魔城伝説を使い、実際のevdevメニュー操作とDRM captureで次を確認した。

- 整数OFF時のphysical表示領域`0,6,480,627`から、ON後の最初のgame復帰で
  `112,153,256,334`へ変わる。
- ON中もFit ScreenのRGUIはphysical `480x640`、logical `640x480`を全面使用する。
- Fit Screenからgame復帰、menu再表示を繰り返しても終了せず、領域も維持する。
- OFF後の最初のgame復帰で`0,6,480,627`へ戻る。

整数倍率自体の選び方は既存のRetroArch回転・axis・aspect方針を維持する。
今回は更新タイミングとmenu独立化を修正した。LCD目視はoperator確認待ち。
整数OFFへ戻した後、operatorがRAを正常終了（rc=0）し、一時watcherがFEを1 processで
復帰させた。最終版の全Runtime checksum検証も合格。deploy時の保存済みcfgは変更せず、
RAの通常終了時には現在のmenu設定が正常に保存された。
release公開、SD image生成は行っていない。
