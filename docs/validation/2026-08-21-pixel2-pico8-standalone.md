# Pixel2 PICO-8 Standalone Validation

Date: 2026-08-21  
Implementation source ref: `18f0fd7`

Validated Device Runtime: `0.1.0-dev-18f0fd7`

## Scope

PICO-8はproprietary runtimeなのでplumOS imageへ同梱しない。ユーザーが所有する
公式AArch64 runtimeをUser FAT32の次のいずれかへ配置すると、PICO-8 systemの
既定`standalone:pico8` routeから起動する。

- `roms/pico-8/aarch64`（canonical）
- `roms/pico8/aarch64`（compatibility alias）

Fake-08とRetro8はFEのalternate routeとして残す。公式runtimeがない公開imageでも
ユーザーがcoreを選び直せるが、公式runtimeがある場合のdefaultはSAである。

## Runtime selection and ownership

入力セットには`pico8_64`という名前のx86-64 binaryと、`_pico8_64`という名前の
AArch64 binaryが混在していた。launcherはファイル名を信用せずELF magicと
`e_machine=0x00b7`を検証し、AArch64だけを選ぶ。

| asset | SHA-256 |
| --- | --- |
| `_pico8_64` | `a2189ef2c500d2d79e1e4358c3176f16f89505cf779d671d757de3163c4f7e` |
| `pico8.dat` | `91212d55b540ef2abf9d5df7bb46fb87f41c35f3e9d108ebd8680debab020be2` |
| plumOS SDL rotation adapter | `06d8b6a99a79a362e92735ee86a01b21a466738464d2540a06ca8c131ba4930a` |

ROM/runtime treeは読取り元として扱い、config、cdata、backup、captureは
`/mnt/plumos/state/standalone/pico8`へ分離した。stop helperが終了を許可する外部binaryは
上記ROM aliasの`aarch64`直下にある3つの既知名だけで、記録PIDの`/proc/PID/exe`も
一致しなければならない。

## Pixel2 contracts

- SDL KMSDRMの論理640x480をnative 480x640へ270度presentする。
- desktop OpenGL rendererはtarget textureを拒否したため、PICO-8だけ
  `SDL_RENDER_DRIVER=opengles2`へ固定する。
- Pixel2 GUIDへABXY swap、D-pad buttons 10--13、Start/Select、shoulder/trigger、
  Function=Guide 14を登録する。
- 音声は共通ALSA `plumos_output`を使用する。
- `-home`、`-desktop`、`-root_path`、`-joystick 0`を明示する。
- 既定は`-pixel_perfect 0`で128x128を1:1のまま480x480へaspect-fitする。
  整数3倍の384x384へ戻す場合だけ`PLUMOS_PICO8_PIXEL_PERFECT=1`を指定する。

## Host verification

- `tests/test-pixel2-emulator-menu-contract.sh`: PASS
- `tests/test-app-layer-scripts.sh`: PASS
- Frontend/Standaloneを並列再生成
- strict app-layer: PASS
- release audit: 88 enabled systems、5 standalone built、0 release blocker
- generated adapter: AArch64 ELF shared object
- component/root checksum: PASS

## Device verification

管理差分は実機の一時領域で全file SHAを検証し、component metadataとroot metadataを
最後に配置した。最終検証はFrontend 198、Standalone 605、root 4265 entryが全合格。

`Celeste.p8`の公式logで次を確認した。

- KMSDRM、renderer `opengles2`;
- `plumOS Pixel2 Controller` 1台、15 buttons、指定mapping;
- `SDL_OpenAudio ok`、22050 Hz mono、ALSA;
- `pico8.dat`読込み、`Celeste.p8` load、`run_cart`到達;
- launcher/runtime PID継続と、記録exeが`_pico8_64`に一致。

DRM primary planeをcaptureし、native 480x640から物理向きへ戻した画像が640x480、
Celesteの128x128画面が歪みなく中央の正方形へ拡大されることを確認した。

`X-Zero.p8`でinteger scaleとaspect-fitを同じDRM primary planeから比較した。
`-pixel_perfect 1`は384x384、`-pixel_perfect 0`は480x480となり、後者も
左右80pxの黒帯を残して正方形を維持した。4:3への横伸びは発生していない。
正式launcherから`-pixel_perfect 0`で再起動し、GLES2、controller、
`SDL_OpenAudio ok`、cart load、継続processを再確認した。

物理D-pad/ABXY/Start pause、実音声、終了、FE復帰はoperator acceptanceを継続する。
