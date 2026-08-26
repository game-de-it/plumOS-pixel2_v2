# Pixel2 RetroArch core aspect validation

## Scope

RetroArchの`Core Provided`を選択しても、mGBAのgameとRGUI menuが正しい比率に
ならない問題を、Pixel2実機`192.168.10.107`で調査・修正した。

## Root cause

問題はmGBA core固有ではなく、Pixel2のRetroArch起動・DRM経路に2段階あった。

1. 通常coreの高優先度append configが4:3を強制し、保存済み
   `aspect_ratio_index = "22"`を上書きしていた。`429e833`で通常coreからaspect強制を
   除去した。
2. RA upstreamは90度frontend回転時にCore Provided geometryを反転する一方、Pixel2の
   DRM backendもscanout時に90度回転する。`9d75fb1`でCore Providedだけ二重反転を相殺した。
3. DRM surfaceは補正済みaspectを保持していたが、game/menu viewport生成時にRA globalの
   補正前aspectを再参照していた。`e4e689b`で両surfaceを確定済みaspectから直接生成した。

## Build and deployment

- source: `e4e689b`
- Runtime: `0.1.0-dev-e4e689b`
- signed delta base: `0.1.0-dev-9d75fb1`
- package SHA-256: `075b92733edf65c99476053e0ac0ea99cce6fcc7504f8c80f00ae04cc93a36a1`
- payload: 10 files, delete 0
- strict app-layer verification: PASS
- license audit: PASS (110 libretro cores)
- device update result: `runtime_healthy`
- device full Runtime verification: `runtime_verify=result-ok`

転送前にdevice modelが`GameKiddy GKD Pixel2`、現行Runtimeが
`0.1.0-dev-9d75fb1`であることを確認した。実機上でpackage SHA-256、署名、base versionを
検査してからatomic renameし、通常updater transactionとsafe rebootで適用した。
ROM、BIOS、save、mutable RA設定は変更していない。保存済みaspectは`22`のままである。

## Device display evidence

mGBA Modernで次を起動した。

`Castlevania - Aria of Sorrow (European Version - Restored Audio).gba`

修正途中のDRM planeは`480x320`となり、3:2 contentが縦長になることを確認した。
最終版では次の通りである。

| surface | physical DRM plane | logical capture | result |
| --- | --- | --- | --- |
| game | 427x640 RG16 | 640x427 | GBA 3:2を保持 |
| RGUI menu | 427x640 XR24 | 640x427 | gameと同一比率 |

保存capture:

- `output/live/2026-08-27-ra-core-aspect/ra-aspect-e4e689b-game-logical.png`
- `output/live/2026-08-27-ra-core-aspect/ra-aspect-e4e689b-menu-logical.png`

game captureは言語選択画面を正立・3:2で表示し、menu captureはQuick Menu全体が
同じ640x427領域へ欠けなく収まった。実機LCDについてもoperatorから
「正常に表示されているように見えます」と合格報告を得た。

## Result

`Core Provided`はmGBA新旧を含む通常RA coreで利用できる。gameとmenuの比率はPixel2 DRM
経路で一致し、固定、custom、FullおよびWonderSwan専用panel rotation契約は変更しない。
