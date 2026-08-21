# Pixel2 RetroArch Menu and Localization Validation

Date: 2026-08-21  
Implementation source ref: `9e580ec`  
Validated device runtime: `0.1.0-dev-9e580ec`

## Scope

Pixel2のRetroArchへRGUI、GLUI（MaterialUI）、Ozone、XMBと追加言語を実装し、
公式RetroArch assetsをapp-layer管理下の`/mnt/plumos/retroarch/assets`へ同梱した。
`menu_driver`と`user_language`はmutable user configを正とし、launcherから固定値を
appendしない。変更はRetroArch再起動後に有効になる。

3.5 inch panelで読めるようfactoryの`menu_scale_factor`を1.5、Ozoneのglobal font
factorを1.35とした。旧factory markerが一致し、3項目が旧既定値のままの場合だけ
限定migrationする。menu driver、language、hotkey、save/stateは変更しない。

## Pixel2 GL coordinate contract

Pixel2のDRM scanoutはnative 480x640、graphical menuはlogical 640x480である。
当初は最終描画だけを回転していたため、XMB layoutは480x640、font/icon描画は
640x480が混在し、日本語文字化けに見えるicon/label重なりが発生した。

次の3経路を同じlogical dimensionsへ統一した。

- menu driver's pre-frame layout callback;
- GL viewport、scissor、menu primitive rotation;
- font glyph sizeとnormalized font position。

## Host verification

- `tests/test-pixel2-retroarch-menu-localization.sh`: PASS
- `tests/test-retroarch-config-merge.sh`: PASS
- `tests/test-app-layer-scripts.sh`: PASS
- RetroArchとFrontendを並列build: PASS
- strict app-layer verification: PASS

## Device verification

管理差分は実機stageでSHA-256を確認し、RetroArch/Frontend component manifest、
component checksum、root manifest/checksumを同じ更新単位で配置した。

DRM primary planeをnative 480x640 XR24で取得し、logical 640x480へ戻して次を確認した。

- XMB Japanese: labelと縦list iconが同じ行へ整列し、文字サイズが判読可能;
- Ozone Japanese: sidebar、main list、説明文、footerが正立し判読可能;
- GLUI Japanese: icon、title、main list、説明文が正立し判読可能;
- RGUI Japanese: built-in language fontで正立表示;
- official XMB/Ozone/GLUI assetsとfallback fontsの欠落なし。

検証後は作業前のactive configをSHA一致backupから復元した。限定migrationにより
表示倍率3項目だけを更新し、`menu_driver=rgui`、`user_language=0`、
`input_enable_hotkey_btn=8`、`input_exit_emulator_btn=9`を保持した。

最終状態は次のとおり。

- Runtime: `0.1.0-dev-9e580ec`;
- RetroArch component checksum: PASS;
- Frontend component checksum: PASS;
- root app-layer checksum: 11266/11266 PASS;
- Frontend process復帰: PASS。
