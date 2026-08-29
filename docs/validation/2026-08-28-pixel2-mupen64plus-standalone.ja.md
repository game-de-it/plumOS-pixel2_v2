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

## SRAM永続化の実機修正

風来のシレン2（ROM ID `NSI`、SRAM 32 KiB）で、ゲーム内セーブ後にFUNCTIONから
正常終了して再起動すると「セーブデータは消えてしまいました」と表示される問題を
実機再現しました。save path、終了signal、save typeの誤りではなく、Mupen64Plus 2.6.0
coreがlittle-endian上でSRAM byteへ`^ 3`を適用する一方、部分書き込み時には元の
unaligned範囲だけをfileへ保存していたことが原因です。DMAが4 byte境界をまたぐと、
更新された直前・直後のbyteが永続化範囲から漏れ、ゲーム側checksumが不正になって
いました。

`eb5499a`でSRAMのfile保存範囲を実際に変更される4 byte word境界まで拡張し、
`0.1.2-dev-eb5499a`の署名Runtime差分を標準updaterで実機へ適用しました。
package SHA-256は
`3196473a3d22426ef44739c15c8733ca4701d36944b25d7103fa9c285490a7cf`、
適用後core SHA-256は
`54c80de1d8da42a3719bb3454726a5cd0755d23f52771b9b7b262ef455367d6b`です。

破損済みSRAMは削除せずdiagnostics配下へ退避し、空のSRAMから次の手順を実施しました。

1. operatorがゲーム内で新規セーブする。
2. SRAMをhostへ回収し、SHA-256
   `e45c0c9003e1a32696cffb70a99a590e03c1b3ca62053362958b5013dde341dd`を記録する。
3. FUNCTION helperと同じSIGTERMで正常終了する。
4. 終了後と再起動後のSRAMが同一SHA-256であることを確認する。
5. operatorがセーブデータを読み込み、続きから開始できることを確認する。

再開後の正常終了では進行に伴ってSRAMが更新され、SHA-256は
`9539838739d1df43b135968c148105232f1fb171ee82036ff1c3915dee4a4598`となりました。
これは消去ではなく、読み込んだセーブでプレイを継続した後の正規更新です。終了後は
FEへ復帰しました。

## アナログ／N64十字切替の実機修正

Pixel2は物理アナログstickを持たないため、standalone Mupen64Plusでは通常の物理十字を
N64 analogとして使用します。アナログとN64十字の両方を必要とするゲーム向けに、
`3aeae5f`でFUNCTION短押しによる可逆切替と1.5秒長押し終了を追加しました。

最初の実機試験では、1回の物理FUNCTION操作が複数のpress/release edgeとして現れるため
切替回数が不定になりました。`45d65ef`でreleaseが200ms安定するまで1 gestureとして扱う
debounceを追加しましたが、終了helperにはraw eventが届く一方、Mupen64Plus SDL input
pluginにはSDL button 14が届かず、切替自体が成立しませんでした。

`96f3af5`ではSDL Function番号への依存を廃止しました。raw evdev
`BTN_TRIGGER_HAPPY1`を監視するhelperが短押しを確定し、対象Mupen64Plus PID専用の
`/run/plumos/mupen64plus-dpad-mode.<pid>`を生成・回収します。input pluginはこの状態だけを
読み、N64十字modeではanalog値を0へ固定して物理十字をN64十字bitへ割り当てます。
長押し終了時とprocess終了時にはmarkerを回収するため、次回起動は必ずanalog modeです。

署名Runtime `0.1.2-dev-96f3af5`を実機へ適用し、transaction `healthy`、host/deviceの
helper・input plugin SHA-256一致、standalone component 640/640 checksum合格を確認しました。
ゼルダの伝説 時のオカリナで通常modeのカーソル移動、FUNCTION短押し後のN64十字mode、
再度短押しした後のanalog復帰をoperatorが実機合格としました。短押しではゲームを終了せず、
ログも`mode=N64 D-pad`と`mode=analog stick`をgestureごとに1回記録します。

## 最終operator acceptance

物理操作、analog／N64十字切替、正方向・4:3表示、FUNCTION長押し終了、FE復帰、2回目起動、ゲーム内SRAMの
保存・読込・続きからの開始を実機で確認しました。音声経路はALSA playback進行を
機械確認済みです。N64既定profileは引き続き`retroarch:parallel_n64`です。
