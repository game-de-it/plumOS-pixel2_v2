# Pixel2 FE環境分離

日付: 2026-08-28  
実機: GKD Pixel2  
source: `508b567`

## 症状

FEからRockboxを起動して正常終了した後、Pyxel systemからPFSを起動すると、音声は
再生される一方で画面が黒になりました。Pyxel processは生存しALSA hardware pointerも
進んでいましたが、activeな480x640 DRM planeは黒1色でした。

PFSとLast EmulatorはPixel2の直接起動経路で合格済みであり、単体のPyxel表示不具合
では説明できない、起動順序に依存する症状でした。

## 原因

PortMaster port launcherが、自身のexport済み環境からFEを復帰していました。このFEは
RockboxとPortMasterの表示adapterを`LD_PRELOAD`に継承し、次のPyxel起動でさらにPyxel
adapter 2件を追加していました。失敗時PFSには次の全系列が同時に入りました。

```text
plumos-pyxel-fit.so
plumos-pyxel-gl-rotate.so
libplumos-portmaster-exec-guard.so
libplumos-portmaster-sdl-rotate.so
libplumos-portmaster-gl-rotate.so
libplumos-portmaster-rockbox.so
```

つまりFEが、直前のapp専用renderer状態を次のappへ運ぶ親processになっていました。

## 修正

`plumos-frontend-launch`は継承したloader状態を破棄し、system path、plumOS root、
runtime rootだけを渡す最小`env -i`環境でfrontend init scriptを起動します。Pixel2の
入力、renderer、ROM、BIOS、device契約は従来通りfrontend initが設定します。

`tests/test-pixel2-frontend-clean-environment.sh`ではloader、SDL、PortMaster、Pyxelの
変数を意図的に汚し、fake frontend initへ一つも到達しないことを検証します。これは
通常のapp-layer script試験に含めました。

## 検証

host試験は全て合格しました。

```text
pixel2_frontend_clean_environment=result-ok
portmaster_pixel2_runtime=result-ok
portmaster_pixel2_rockbox=result-ok
portmaster_pixel2_session_cleanup=result-ok
app_layer_verify=result-ok strict=1
```

exact-source署名Runtime差分を通常updaterから適用しました。

```text
runtime=0.1.2-dev-508b567
package_sha256=cddeafd5c070448e2d18fc8802a64955bf1b276938a1eea5b888d078ccd97c57
payload_files=10
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
```

修正後、復帰FEには`LD_PRELOAD`も`PLUMOS_PORTMASTER_*`変数も残りません。続くPFSは
想定するPyxel adapter 2件だけを読み込み、DRM planeは非黒画面、ALSAは`RUNNING`のまま
pointerが進み、実LCD表示もoperator確認に合格しました。

証拠hashは次の通りです。

```text
9347fe86f6b448518e30d671057b4ece3e7f2cf3906488168f63736a4c990bc0  pfs-black-logical.png
1958ca95e4c0b8d46a7472f3fb2d3c5702e83f3b71fe50db6ffd7681e9fa9398  pfs-clean-logical.png
```

この修正は特定port専用ではありません。PortMaster title終了後に起動する全emulator/appは、
直前portのprivate表示libraryではなく、自身のlaunch profileだけを受け取ります。
