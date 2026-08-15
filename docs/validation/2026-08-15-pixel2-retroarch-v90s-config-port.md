# Pixel2 RetroArch factory configuration port

## Scope

V90Sで採用している3,374-key RetroArch factory configurationを基準に、Pixel2の
display、input、audio、storage contractへ移植した。V90S固有hardware値やidentityを
コピーするのではなく、共有RetroArch機能面をPixel2へ導入することを目的とした。

## Pixel2 adaptations

次の実機確認済みPixel2 contractを維持した。

- DRM、固定panel rotation 3、60 Hz、threaded video、global 4:3。
- udev `pixel2_joypad`、物理A/B対応、D-pad button 10-13、FUNCTION button 14。
- ALSA `plumos_output`、48 kHz、96 ms latency。
- BIOS `/mnt/plumos-user/bios`、screenshot/recordingはFAT32 user volume。
- save/stateはlauncherがsystem別のext4 pathを指定し、ROM隣接保存は無効。
- networking無効buildで動かないCore/Online Updater menuは非表示。

その上でV90S factory surfaceからautomatic override/remap、playlist/history、RGUI、
recording、thumbnail、directory、savestate、hotkey、configuration persistenceを
導入した。factory cfgは3,374 keyすべてが一意で、Mali fbdev、home-relative path、
他機種identityを含まないことをhost testで検証する。

## Mutable configuration migration

`plumos-retroarch-config-merge`をgame/menu launcher共通の入口に追加した。

- cfgが無い場合はfactoryをatomic installする。
- 旧Pixel2 57-key factoryの既知SHA-256と完全一致する場合だけ完全版へ置換する。
- 利用者が変更したcfgは既存値を上書きせず、factoryにある不足keyだけを補う。
- factory SHA markerにより同じversionのmergeを毎回行わない。

`config_save_on_exit = true`を有効にし、runtime appendから強制falseを除外した。
一方、表示回転、udev、audio、system別save/stateなどhardware-critical値は従来通り
launcher appendが毎回保証する。

## Host validation

Commit `aa3a3ab`からversion `0.1.0-dev-aa3a3ab`としてfrontend、RetroArch、strict
app-layerを再生成した。

```text
app_layer_scripts=result-ok
PASS: Pixel2 emulator FUNCTION menu contract
retroarch_config=result-installed
retroarch_config=result-merged added=3373
app_layer_verify=result-ok
app_layer=result-ok strict=1
```

BusyBox fixtureでは新規installと、改行なしのuser cfgを含むmissing-key mergeを実行し、
user指定`video_driver = "custom"`を保持したまま3,374 unique keyへ到達した。

## Signed device deployment

稼働中`0.1.0-dev-b370bfa`の実機checksumをbaseに、16管理file、削除0の署名Runtime
deltaを生成した。

```text
runtime=0.1.0-dev-aa3a3ab
package_sha256=089bd1003bc32b071f5ce0862ec2bfa319013354581e32491c6c681970f06183
transaction_status=healthy
runtime_pending=absent
runtime_verify=result-ok
root_checksums=4241/4241
```

更新前のmutable cfgは旧factory SHA
`b97c897bb7141a77d6fb17e77a66895540b9bd9cc177d7ee983afcb75e5e819e`と一致したため、
helperが完全版へ置換した。更新後はmutable/factory/markerがすべて
`9f4aaebdab3cc3a9be24b203161e63f63f1887e4eb0b79c82618086d3cbc4b24`で一致し、
3,374 unique key、重複0を確認した。

RGUI menu smokeではRetroArchが`/dev/dri/card0`と`/dev/input/event2`を取得して稼働し、
Pixel2 cfgをparseできることを確認した。終了後はFEとhardware-key serviceがそれぞれ
1 processで復帰した。物理FUNCTION操作と各core固有override/remapの目視acceptanceは、
個別emulator検証gateとして区別する。
