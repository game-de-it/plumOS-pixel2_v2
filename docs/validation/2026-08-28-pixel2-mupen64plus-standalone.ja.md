# Pixel2 standalone Mupen64Plus検証

日付: 2026-08-28

## 対象

Nintendo 64の追加profileとしてstandalone Mupen64PlusをPixel2へ実装しました。
実機実績のある`retroarch:parallel_n64`は既定のまま維持します。対象外にした
Mupen64Plus-Next libretro coreを復活させる変更ではありません。

standalone版はupstream 2.6.0のconsole UI、core、SDL audio、SDL input、HLE RSP、
Rice videoの6 componentをfull commitで固定します。Pixel2側ではnative 480x640
panel向けRice GLES2最終回転、`pixel2_joypad` mapping、process所有権を検証する
FUNCTION終了、plumOS ALSA経路、mutable設定・screenshotの分離、component metadata、
checksum、licenseを統合しました。

## host検証

Mupen64Plus単独buildと、既存emulatorを含む6本の完全standalone buildが成功しました。
core API build後の独立した4 pluginは並列buildします。完全app-layerの監査結果は次の通りです。

```text
standalone: 6 built / 4 libretro-covered deferred / 0 pending
release blockers: 0
```

次の試験に合格しています。

```text
./tests/test-app-layer-scripts.sh
./scripts/build-standalone-pixel2.sh --filter mupen64plus
./scripts/build-standalone-pixel2.sh
./scripts/build-frontend-component.sh
./scripts/build-app-layer.sh --strict
./scripts/audit-pixel2-implementation.py --release-gate
```

strict app-layer検証ではstandalone componentとrootの全checksumを確認しました。
license監査は168 fileを検証しています。AArch64 container上ではconsole、core、
4 pluginのdynamic dependencyがすべて解決し、固定coreを指定した
`mupen64plus --help`も成功しました。

```text
standalone checksums: 9906b53fb48d0ccbfb93694f2773e0aab261ef451b95c83678ec5d3f0387523b
root checksums:       9ea508d63560f8c11418e8fbb62bba694fb70bc73663df1fcfab9063e9818e20
```

## 実機acceptance境界

今回の確認中は既知のPixel2 IPへSSH接続できなかったため、実機deployとN64の起動、
画面向き・aspect、十字キー・各button、音、FUNCTION終了、FE復帰、user設定を保持した
2回目起動は未確認です。

