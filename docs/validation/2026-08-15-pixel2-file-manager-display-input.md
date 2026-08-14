# Pixel2 File Manager display and input validation

## Scope

Pixel2 の File Manager（NextCommander）で、表示の右側が欠け、物理操作が
不自然になる問題を実機上で再現し、MF/V90S と同じ foreground app contractを
Pixel2 の確定済み controller orderへ適合させた。

## Root causes

Pixel2 portはMFのNextCommander adapterを基にしていたが、MF固有のSDL button
orderまで継承していた。Pixel2の共有input mapは次の順序である。

```text
A=1 B=0 X=2 Y=3 L=4 R=5 SELECT=8 START=9
UP=10 DOWN=11 LEFT=12 RIGHT=13 FUNCTION=14
```

修正前はA/Bが逆で、D-padを11-14として扱っていた。このためUPは未処理、
DOWN/LEFT/RIGHT/FUNCTIONはそれぞれUP/DOWN/LEFT/RIGHTとして処理されていた。

表示側には別の不具合があった。Pixel2の物理panelは480x640で、File Managerは
論理640x480をCCW回転して表示する。共有fbdev rendererのpixel境界判定が回転前も
物理幅480を使用したため、論理x=480..639が描画されず、画面右160 pixelが黒く
欠けていた。

## Fixes

- `9b4070d`でNextCommanderをPixel2の共有input mapへ合わせた。
  - A=open、B=parent/cancel、X=operation、Y=system
  - L/R=page、SELECT=select、START=transfer、FUNCTION=system menu
  - D-padはSDL button 10-13
- 右paneの既定pathを共通ROM mount `/roms`へ変更し、headerにも実体を表示する。
- `0106a75`で90/270度回転時の論理境界をphysical height x physical widthとして
  検証し、回転後のphysical境界も再検証するよう共有rendererを修正した。

## Build and signed deployment

両修正は個別commit、再現NextCommander build、strict app-layer assembly、署名
Runtime deltaとして適用した。

```text
9b4070d package_sha256=a119069ecdb80a012b4693375efca8624b4b7108711f67b858e9d85d6946d948
0106a75 package_sha256=67cf99c4a7c0a1da99cf97c0c09a154f42f29bb640bd9520ed7322831f6823af
final_runtime=0.1.0-dev-0106a75
update_result=runtime_healthy
```

更新はmanaged app-layerだけを対象とし、ROM、BIOS、save、frontend/user settingは
変更していない。

## Device evidence

最終DRM planeは物理480x640 `XR24`、表示方向へ回転したcaptureは640x480全域が
非黒pixelを持ち、左pane `/mnt/plumos-user`、右pane `/roms`を完全表示した。

実機の `/dev/input/event2` (`pixel2_joypad`) へ通常のEV_KEY eventを入力し、
NextCommanderが実際に使用するSDL経路で次を確認した。

- DOWNでhighlightが`..`から`.fseventsd`へ1行下がる。
- FUNCTIONでSystem menuが開く。
- BでSystem menuをcancelし、directory内ではparentへ戻る。
- Aでhighlight中のdirectoryをopenする。
- FUNCTION、DOWN x4、AでQuitし、frontendを再取得できる。

画面証拠はGit管理外の次のdirectoryへ保持する。

```text
output/live/2026-08-15-file-manager-audit/
  file-manager-0106a75-display.png
  fm-0106a75-down-display.png
  fm-0106a75-function-display.png
  fm-0106a75-b-display.png
  fm-0106a75-a-display.png
```

ホストの`tests/test-app-layer-scripts.sh`、NextCommander component checksum、strict
app-layer verifyに合格した。更新後の実機でも`plumos-system-update verify-runtime`が
全managed checksumに合格した。実物の物理buttonによる最終目視はoperator gateとして
この機械検証と区別する。
