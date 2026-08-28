# Pixel2 standalone Mupen64Plus検証

日付: 2026-08-28

## 対象

Nintendo 64の追加profileとしてstandalone Mupen64PlusをPixel2へ実装しました。
実機実績のある`retroarch:parallel_n64`は既定のまま維持します。対象外にした
Mupen64Plus-Next libretro coreを復活させる変更ではありません。

standalone版はupstream 2.6.0のconsole UI、core、SDL audio、SDL input、HLE RSP、
Rice videoの6 componentをfull commitで固定します。Pixel2側ではnative 480x640
panel向けRice GLES2最終回転、`pixel2_joypad` mapping、process所有権を検証する
FUNCTION終了、plumOS ALSA経路、mutable設定・runtime data・screenshotの分離、
component metadata、checksum、licenseを統合しました。Riceが書き換えるROM別設定DBは
起動ごとに`/run`へ作業コピーを作り、package済みdata directoryを可変状態として
pluginへ渡しません。

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
standalone checksums: 6862960baacaf2e8a9b839e481c3b62cd00ff88714ecf89fde486c72cf9a6457
root checksums:       ab1280690eca94707080cf82e84765a6adc5ad66aec9cebc8bc291b64826932c
```

## 実機deployと機械acceptance

`0.1.2-dev-198acfa`から`0.1.2-dev-d4ab428`への署名Runtime差分を通常updaterで
実機へdeployしました。package SHA-256は
`9beceea8c4dbffb79eccd678ff082afb7c1dfda92c44a4b9841e3c33424d6df2`です。
実機は署名とexact source versionを受理して再起動し、request・pending stateを回収、
target versionとsource refへ移行しました。起動試験の前後でinstalled Runtime全検証に
合格しています。

最初の実機試験ではhostだけでは検出できなかった次の2件を発見しました。

- Rice側のGL symbol取得順ではadapterがreal `SDL_GL_GetProcAddress`を解決できず、
  libraryはload済みでも最終回転が初期化されずcaptureが横倒しだった。
- 終了時にRiceがpackage済み`RiceVideoLinux.ini`を書き換え、11,328件のRuntime
  checksum中1件が不一致になった。

`9495fa5`でclientのsymbol取得順に依存せずSDL helperを解決し、`d4ab428`で5本の
Mupen64Plus shared dataをlaunch-private cacheへ配置して終了時に回収します。適用後logは
次の初期化を記録しました。

```text
[plumOS] Mupen64Plus GL rotation context: OpenGL ES 3.1 Mesa 22.3.6
[plumOS] Mupen64Plus GL rotation: logical=640x480 present=480x640+0+0 scanout=480x640 @ 270
```

実機media validatorは`N64/SUPERMARIO64.Z64`を`standalone:mupen64plus`で起動し、
84% batteryで`startup=pass`、`screen=pass`、`audio=pass`となりました。capture目視で
論理画面は正しい横向き640x480・4:3、物理scanoutはpanelに必要な480x640回転を確認。
ALSA playbackは進行し、`pixel2_joypad` profile選択とFUNCTION helperによる
`BTN_TRIGGER_HAPPY1`監視も確認しました。試験終了後はFEへ復帰し、一時data directoryは
0件、package済み`RiceVideoLinux.ini`はSHA-256
`ec89fe5ab5760b94b822b41dc2889afc280a138fe1cb43811e1346b539850be9`を保持し、
最後のRuntime全検証にも合格しました。

## 残るoperator acceptance

物理十字キー・各button、実際の音質、FUNCTION終了、FE導線からの2回目起動はoperator
確認として残します。N64既定profileは引き続き`retroarch:parallel_n64`です。
