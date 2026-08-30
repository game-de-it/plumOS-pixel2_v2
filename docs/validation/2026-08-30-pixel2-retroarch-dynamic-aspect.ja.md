# Pixel2 RetroArch 動的アスペクト変更の検証

## 対象

ゲーム実行中にRetroArchのアスペクト比を`Core Provided`へ変更してもゲーム画面へ
反映されず、その後に開いたメニューだけが新しい比率になって乱れて見える問題を、
Pixel2実機`192.168.10.107`で調査・修正した。

再現にはGambatteと`Aretha (Japan).gb`を使用した。保存設定を4:3
（`aspect_ratio_index = "0"`）にした起動直後は、ゲーム用DRM planeが物理
`480x640 RG16`だった。実行中に`Core Provided`（index 22）へ変更してもゲーム面は
`480x640`のまま変化せず、メニューを開いた時だけ`480x533 XR24`へ変わった。

## 原因と修正

Pixel2のplain DRM backendは、アスペクト比に合わせた寸法のdumb bufferへゲームと
メニューを描画する。runtimeの`drm_set_aspect_ratio()`は新しい比率を保存するだけで、
作成済みsurfaceを再生成していなかった。このため、既存のゲームsurfaceは旧4:3、後から
作られたメニューsurfaceはGBの比率になっていた。

`6777ec6`ではruntimeの比率変更時に次を行う。

- main surfaceとmenu surfaceを解放し、次のframeで新しいviewport寸法から再生成する。
- core寸法cacheを無効化する。
- menuの開閉状態は変えず、開いているmenuも新しい寸法で再生成する。

`0292cc5`では新規installとFactory Reset用のRetroArch factory defaultを
`Core Provided`（index 22）へ変更した。launcherのappend configは通常coreの比率を
上書きしない。既存active configに`aspect_ratio_index`があればconfig mergeはその値を
変更しないため、ユーザーが4:3、Full、Customなどへ変更した後の設定が優先される。

## 実機結果

| 試験 | DRM game plane | 結果 |
| --- | --- | --- |
| 修正前、4:3で起動 | `480x640 RG16` | baseline |
| 修正前、実行中にCore Providedへ変更 | `480x640 RG16` | 変更が反映されない |
| `6777ec6`適用後、実行中に変更 | `480x533 RG16` | GB比率へ再生成 |
| `0292cc5`適用後、保存済み22で起動 | `480x533 RG16` | 起動時からCore Provided |

operatorは修正後のゲーム画面とRGUIメニュー画面を目視し、両方とも正常な
アスペクト比であることを確認した。最終のゲームcaptureはnon-black ratio `1.0`で、
FEへ復帰後もactive configはindex 22を保持した。

証跡は
`output/live/2026-08-30-pixel2-retroarch-aspect-regression/`に保存している。

## Buildとdeploy

- Runtime: `0.1.4-dev-0292cc5`
- dynamic surface修正: `6777ec6`
- factory default修正: `0292cc5`
- clean managed delta SHA-256:
  `3247de985761de9f1a1aff17625a87eb2c075793f5ec458d1b59ee3dc0ff4771`
- strict app-layer build: PASS
- license audit: PASS（libretro 110 core）
- 実機RetroArch component: 7,060 / 7,060 checksum一致
- 実機app-layer: 11,333 / 11,333 checksum一致

deploy対象は7管理fileとroot `checksums.sha256`だけである。active
`retroarch.cfg`のSHA-256はdeploy前後とも
`df46c83539f2269f0d7247240c4c042097f84577add3d0df16a1237658635c50`で一致し、
ユーザー設定、ROM、BIOS、save、stateは上書きしていない。
