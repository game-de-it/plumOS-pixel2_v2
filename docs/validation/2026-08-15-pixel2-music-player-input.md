# Pixel2 Music Player input validation

## Scope

Pixel2のMusic Player button mappingを、共有Pixel2 input contract、MF/V90Sの
Music Player仕様、稼働中実機のevdev logで再確認した。

## Root cause

MF/V90S版はcontrollerが報告するLinux `BTN_A` / `BTN_B`を、そのまま物理A/Bとして
扱える。Pixel2 kernel/input contractは次のように物理labelとLinux aliasが逆になる。

```text
physical A = code 305 = BTN_EAST = BTN_B
physical B = code 304 = BTN_SOUTH = BTN_A
```

移植後のMusic Playerは`BTN_A`を再生、`BTN_B`を終了に割り当てていたため、実機では
物理Bが再生、物理Aが終了になっていた。旧Runtime logでもcode 305で
`fast exit requested`、code 304で`track loaded`を再現した。またPixel2 FUNCTIONの
code 704 (`BTN_TRIGGER_HAPPY1`) はhandlerに無く、操作不能だった。

## Corrected Pixel2 contract

`b370bfa`でPixel2専用patchを次の物理操作へ合わせた。他platformの処理は変更して
いない。

| Physical control | Linux code | Music Player action |
| --- | --- | --- |
| D-pad Up/Down | 544/545 | selection and list scrolling |
| D-pad Left/Right | 546/547 | seek -5/+5 seconds |
| A | 305 `BTN_EAST` | play/pause selected track |
| B | 304 `BTN_SOUTH` | fast clean exit |
| X/Y | 307/308 | previous/next track |
| START | 315 | play/pause selected track |
| SELECT | 314 | cycle EQ preset |
| L/R | 310/311 | app volume down/up |
| FUNCTION | 704 | fast clean exit |

on-screen helpとcomponent READMEも`A Play/Pause`、`B/FUNCTION Exit`へ同期した。

## Build and signed deployment

source patchのinput contract test、再現Music Player build、component checksum、strict
app-layer assemblyに合格した。稼働中`0.1.0-dev-883fd1d`をbaseに署名Runtime deltaを
生成し、通常のinspect、request、safe reboot経路で適用した。

```text
runtime=0.1.0-dev-b370bfa
package_sha256=e53f90ec7423d0907e457323921c3742db2aebcfeb27d64deb138a7dc38c60ba
transaction_status=healthy
music_binary_sha256=d5098e5117e6924cca765f69efbf7906e5db404e37d9cb2d7f5deec63195a7f3
runtime_verify=result-ok
```

## Device proof

既存`Clock.wav`を元に2個の明示的な一時copyを作り、3曲listで実際の
`/dev/input/event2`へ物理buttonと同じEV_KEY eventを入力した。Music Player自身の
input/audio/state経路で次を確認した。

```text
scan total tracks=3
code=305 name=BTN_EAST/physical-A -> track loaded index=0
DOWN + A -> track loaded index=1
UP + A -> track loaded index=0
RIGHT/LEFT -> seek input accepted
X/Y -> previous/next track load
code=310 BTN_TL -> app volume=75
code=311 BTN_TR -> app volume=80
code=304 BTN_SOUTH/physical-B -> fast exit requested by input
code=704 BTN_TRIGGER_HAPPY1/FUNCTION -> fast exit requested by input
```

最初のA再生では`audio write ok frames=768 bytes=3072`も記録された。SELECTとSTARTも
正しいcodeとして受理された。B終了とFUNCTION終了は別sessionで検証し、いずれも
processが終了した。

検証用2 fileと一時log markerを削除し、元の`Clock.wav`はSHA-256
`e3c796dccc4802ed9f72ca10a1234b01e5bf9783e754e894116f05a153cb8ce5`のまま保持した。
最後にMusic Player component checksumを全件確認し、FEとhardware-key serviceを
通常状態へ戻した。実物buttonによる最終目視はoperator gateとして、このEV_KEY
経路検証と区別する。
