# Pixel2 device startup/display/audio smoke (2026-08-22)

## Scope

Wi-Fi dongleと充電器を同時に使えない実機制約の中で、現在のFAT32 user
partitionにROM cacheが存在する17 system・42 launch profileを検査した。
ROM、BIOS、save、設定は変更していない。検査用DRM capture binaryだけを
`/tmp`へ置き、各route後にFEを復帰した。

- device: `root@192.168.10.110`
- System: `0.1.0-dev-4993d8c`
- Runtime: `0.1.0-dev-16150d5`
- battery: 98% -> 93%（最終確認後もFE running）
- validator: `scripts/validate-pixel2-device-media.py`
- current FE cache: 88 enabled中17 systemにROMあり、71 systemはROMなし

## Acceptance boundary

機械判定は次の3点だけを合格条件とした。

1. launcherのprocess group内で実emulator processが継続し、zombieではない
2. Pixel2 DRMのpanel-size planeに非黒・非単色の実画像が存在する
3. ALSA playbackが`RUNNING`で、1秒間に`hw_ptr`が進行する

64x64 hardware cursor plane、終了済みzombie、FE残像は証拠から除外した。
Rockchip `fb0`はDRM clientの実scanoutを返さない場合があるため、OpenBORだけ
補助的に直接readしたが全黒だった。

この検査では入力、menu/exit、save/load、実際に聞こえる音量・音質・音飛び・
同期、動的なちらつき、厳密なaspect ratioは判定しない。静止画一覧では成功38
routeに明らかな90/180度回転や画面外への全体逸脱は見えなかったが、ゲーム操作中の
目視acceptanceを代替しない。

## Result

| result | route count | detail |
|---|---:|---|
| machine pass | 38 | startup、DRM image、ALSA pointerの全て合格 |
| manual display | 1 | OpenBOR: startup/audio合格、DRMとfb0は全黒 |
| launch fail | 3 | Channel F、ColecoVision、VMU |
| no cached ROM | 71 systems | 今回は起動対象外 |

機械合格したrouteは、NES 6、GB 8、Mega Drive 4、Game Gear 6、32X 2、
PSP 1、N64 1、Dreamcast 2、EasyRPG 1、PICO-8 3、Pyxel 1、Cannonball 1、
Amstrad CPC 2の合計38 profile。RetroArch、PicoArch、PPSSPP、official
PICO-8、Fake-08、Retro8、Pyxelの各実行familyを含む。

### Clear failures

- Channel F / `retroarch:freechaf`: processは15秒前に終了し、RA logは
  `retroarch=result-exit rc=0`。必須BIOS
  `sl31253.bin`、`sl31254.bin`、`sl90025.bin`は現在のuser BIOS rootにない。
- ColecoVision / `retroarch:bluemsx`: text UIは`execute: failed`、RA logは
  `retroarch=result-exit rc=1`。BlueMSXのColeco BIOSはROM tree側にはあるが、
  RA `system_directory=/mnt/plumos-user/bios`から利用できる配置になっていない。
- VMU / `retroarch:vemulator`: RA logは`Segmentation fault`、
  `retroarch=result-exit rc=139`。現在のcontentは108-byte `ANIMTEST.VMI`だけで、
  paired dataも含めてcontent契約の再確認が必要。

### Manual queue

- OpenBOR `Dragon Ball [v.3.0 Build 4086].PAK`の実LCD表示。OpenBOR processは
  継続し、ALSA `hw_ptr`は進行するが、DRM planeと`/dev/fb0`は全黒だった。
- 成功38 routeの可聴音、音質、音飛び、映像同期。
- 成功38 routeの操作中の向き、aspect ratio、欠け、色、ちらつき。
- 入力、function menu、終了、FE復帰、save/loadは今回の検査範囲外。

## Evidence

- `output/validation/pixel2-device-media-2026-08-22-defaults-final/`
- `output/validation/pixel2-device-media-2026-08-22-alternates/`
- `output/validation/pixel2-device-media-2026-08-22-recheck/`
- `output/validation/pixel2-device-media-2026-08-22-flycast-recheck/`
- `output/validation/pixel2-device-media-2026-08-22-sega32x-recheck/`
- `output/validation/pixel2-device-media-2026-08-22-openbor-fbcheck/`
- `output/validation/pixel2-device-media-2026-08-22-contact-sheet-final.png`

各route directoryにはlaunch log、process table、DRM plane metadata/raw/PNG、
ALSA 2 sample、battery残量、JSON/Markdown reportを保存した。
