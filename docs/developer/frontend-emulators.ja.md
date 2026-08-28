# フロントエンドとエミュレータ統合

[English](frontend-emulators.md)

## FE data model

Pixel2 FEは次を読みます。

```text
/mnt/plumos/config/frontend/systems.json
/mnt/plumos/config/frontend/menus.json
/mnt/plumos/config/frontend/apps.json
/mnt/plumos/state/frontend/library-index.json
```

START menuはplumOS共通の6項目です。

1. UI Settings
2. System Settings
3. Network Settings
4. Apps
5. HELP
6. POWER

`POWER`はSleep/Reboot/Shutdown/Cancelの共通menuを開きます。START menuへRebootと
Shutdownを重複表示しません。Appsにはapp-layer componentが存在するtoolだけを公開し、
実体のないlauncherを表示しません。

## Launch profile契約

```text
retroarch:<core-id>
picoarch:<core-id>
standalone:<emulator-id>
pyxel:pixel2
```

生成app-layerにruntime/coreが存在するsystemだけをFE catalogへ公開します。
`plumos-text-ui launch ... --no-scan`はhostと実機のlaunch-plan検証に使用します。

## RetroArch

RetroArchとlibretro coreはAArch64でpackage化します。full catalogは次でcoreごとのworkerを
並列実行し、canonical Pixel2 componentへ集約します。

```sh
./scripts/docker-build.sh core-catalog --filter all --concurrency 4
```

標準setは42 core、full catalogは110 coreです。従来mGBAは
`4f70b313fcf82b043bee232dd5af231a7755e1d8`の`mgba_libretro.so`として維持し、
新しい固定版`e31759b24e7a4e3899285ff720d7b573ac328ae7`は
`mgba_modern_libretro.so`として独立package化します。新コアは`mGBA Modern`と表示し、
system既定値は変更しません。バッテリーセーブは共有し、互換性が保証されない
ステートセーブだけを専用directoryへ分離するため、同じROMで安全に性能比較できます。

RK3326性能方針によりSaturnは`unsupported_performance_rk3326`としてbuild・表示しません。
Mupen64Plus-Nextもstock kernel上の各renderer/interpreterでsegfaultしたためlibretro版には
採用しません。N64の既定は実機確認済み`retroarch:parallel_n64`のまま維持し、追加選択肢として
`standalone:mupen64plus`を提供します。standalone版はupstream 2.6.0の6 componentを固定し、
Rice GLES2、`pixel2_joypad`、logical 640x480からnative 480x640への最終回転、FUNCTION終了を
Pixel2契約として実装します。

### 画面とaspect

stock kernelはLCDをnative `480x640` DRM modeとして公開しますが、plumOSはFEとemulatorへ
logical `640x480` landscapeを提供します。RetroArch DRM backendはsoftware rotationを持ち、
Pixel2 defaultは`video_rotation = "3"`（CCW）です。

factory defaultは4:3（`aspect_ratio_index = "0"`、`video_force_aspect = "true"`）です。
通常coreのlauncherは高優先度append configへaspect値を再指定しないため、RAの
`Settings -> Video -> Scaling -> Aspect Ratio`で選んだ値が次回起動にも残り、mGBAなどの
`Core Provided` geometryも利用できます。未変更時はNESなども従来の4:3で起動します。
RA upstreamは90度frontend回転時にcore geometryを反転しますが、Pixel2 DRM backendも
scanout生成時に回転するため、そのままでは二重反転になります。`Core Provided`の場合だけ
DRM側で反転を相殺し、固定・custom・Full比率はlogical landscape値のまま扱います。
game surfaceとmenu surfaceのviewport計算も、補正前のRA global値ではなくDRM surfaceへ
確定した比率を使用します。これによりgameとRGUI menuが同じ正しい比率になります。
WonderSwan/WonderSwan ColorだけはSELECTによるcontent回転とpanel補正を分離します。
profileは`video_rotation = "0"`、`video_allow_rotate = "false"`、
`PLUMOS_DRM_PANEL_ROTATION=3`、core-provided aspect `22`を使用し、横224x144と縦144x224の
両方を正しい向き・比率で表示します。

### 入力とhotkey

Pixel2 RAは`udev` joypad driverを使用します。十字はaxisを推測せず、capture済みinput
contractと生成autoconfigに従います。Functionはcompact udev button 14でmenuを開きます。
SELECTをhotkey enableとして次を提供します。

| 操作 | 機能 |
| --- | --- |
| START + SELECT | RA終了 |
| SELECT + L / R | state load / save |
| SELECT + X | screenshot |
| SELECT + Y | FPS表示 |
| SELECT + 十字左 / 右 | state slot変更 |
| SELECT + L2 / R2 | slow / fast motion |

Pixel2のL2/R2はbutton 6/7です。他機種のtrigger-axis設定をコピーしません。

factory contractは`retroarch.cfg`だけでなく、次のbundleです。

- `retroarch.cfg`: global input、save、display、audio、menu
- `retroarch-core-options.cfg`: core option default
- `remaps/<core>/<core>.rmp`: core別controller remap

`plumos-retroarch-config-merge`は欠けているkeyだけを補い、既知の旧factory値だけを世代と
値の両方が一致する場合に移行します。利用者のmenu、language、hotkey、save/state設定を
上書きしません。

RGUI、GLUI/MaterialUI、Ozone、XMBをbuildします。defaultはRGUIです。選択はRA自身の
`Settings -> Drivers -> Menu`と`Settings -> User -> Language`が正で、launcherのappend
configから強制しません。Appsとgame launcherの両方が保存済み選択を読み、RGUIは軽量な
plain DRM、GLUI/Ozone/XMBはPixel2固定回転を持つGLES経路を使用します。graphical assetは
`/mnt/plumos/retroarch/assets`に置き、CJKを
含むupstream fallback fontを収録します。logical 640x480にlayout、viewport、font座標を
揃え、XMB icon/labelの重なりを防ぎます。Ozone、XMB、MaterialUIがcursor/icon用に複製する
独自行列にも、その複製前に固定panel回転を反映します。factory menu scaleは1.5、Ozone
font scaleは1.35です。

save/stateはROM filesystemを優先し、content folder/coreで整理します。content-localを
無効にした場合は`/mnt/plumos/saves/<system>`と`/mnt/plumos/states/<system>`をfallbackに
します。migrationは既存fileを削除しません。

## Runtime family

### PicoArch

生成済み`/mnt/plumos/cores/*_libretro.so`を共有し、Pixel2 ALSA `plumos_output`を使用します。
Functionは`BTN_TRIGGER_HAPPY1`を`EACTION_MENU`/`PBTN_MENU`へbindします。Pixel2 Functionを
`BTN_MODE`として扱いません。

### Standalone

- DraStic: steward-fu/nds統合、armhf coreとpackage-local library、armhf ALSA plugin。
  利用者BIOSだけを`/mnt/plumos-user/bios`以下からmutable work directoryへcopyします。
- PPSSPP: pinned upstream v1.20.4のSDL2/GLES/EGL build。touchを無効にしたPixel2 input、
  readable menu scale、logical landscapeと最終CCW scanoutを分離します。
- PCSX-ReARMed: pinned r26l、NEON renderer、libpicofe raw evdev、48 kHz RK817 route。
  物理A決定/B戻る、Function menu、画面、入力、音声、終了、FE復帰を実機確認済みです。
- OpenBOR: Pixel2 SDL route。FunctionはSDL button 10からEscape/menuへ対応します。

DraStic/PPSSPP/PCSX/OpenBORはいずれも利用者設定とsaveをmutable領域へ保持し、app-layer
updateで上書きしません。BIOSをreleaseへ同梱しません。

### Function menu共通契約

全runtimeは物理Function、raw evdev code 704（`BTN_TRIGGER_HAPPY1`）からnative menuを
開けなければなりません。framework indexは実機captureから導出します。

| runtime | menu経路 |
| --- | --- |
| RetroArch | compact udev 14 |
| PicoArch / PCSX | libpicofe evdev action |
| DraStic | SDL button 8、action 1032 |
| PPSSPP | SDL GuideからBack/Pause code `10-4` |
| OpenBOR | SDL button 10からEscape/menu |

`tests/test-pixel2-emulator-menu-contract.sh`がmapping消失と`BTN_MODE`への巻き戻りを防ぎます。
NDSは存在しないDeSmuME routeを公開せず、`standalone:drastic`を使用します。ScummVM、
EasyRPG、Flycast、NXEngine-Evoはlibretro routeを製品経路とし、standalone版は明示的に
scopeを変更しない限り未実装扱いにしません。

### Pyxel、PICO-8、Apps、Ports

PyxelはPython 3.11、固定wheel、SDL2 KMSDRM/GLES、display-fit shim、`plumos_output`を含む
Pixel2 componentです。FEは`pyxel:pixel2`から`bin/plumos-pyxel-pixel2-launch`を解決します。
Pyxel Setupは`/roms/pyxel/requirements.txt`の任意dependencyをmutable stateへinstallし、
package runtimeを上書きしません。PICO-8は利用者が用意したruntimeを所定content pathから
起動し、releaseへproprietary binaryを含めません。launcherはELF magicと
`e_machine=0x00b7`を検査してAArch64 binaryだけを選びます。

PICO-8の`use_wget 1`はpackage-local adapterへ接続します。stock BusyBox ashはPATHより
builtin wget appletを優先するため、PICO-8 processだけへpreloadする`system()` shimが
先頭の`wget `を絶対pathへ置換します。adapterはPICO-8が使用する`URL`、`-q`、`-O`、
`--post-file`だけを受け、plumOS管理下のcurl/CA、redirect、timeout、retryを使用して
一時fileから原子的に確定します。system-wide wgetや他processには影響しません。
query文字列はlogへ出さず、method、endpoint、出力先、結果、byte数だけを記録します。
runtime、cartridge、config、cdataはuser-ownedのままで、app-layer checksum対象へ
取り込みません。

PortMaster、File Manager、Music Playerはmanifest/checksum管理されたPixel2 componentです。
foreground所有、回転表示、入力、必要な音声、終了、FE再取得を実機確認済みです。
PortMasterでinstallしたgameは利用者所有のmutable dataとして保持し、assembly/updateで
置換しません。

install後または内容変更後の初回起動時だけ、AArch64 ELF依存closureとhost環境依存を
監査します。OS boot時には実行しません。共通exec guardはport固有libraryを保持しながら、
すべての子processへPixel2のlibrary、表示、session契約を復元します。終了時は同じsession
identityでbackground helperを回収してからFEを再取得します。詳細は
[PortMaster共通互換レイヤー](../validation/2026-08-27-pixel2-portmaster-compatibility.ja.md)を
参照してください。
