# TODO

## Adopted architecture

- [x] 既存plumOS機のupdate/storage/frontend/emulator設計を調査する
- [x] Pixel2のRockchip prefix + System A/B + ext4 Runtime + FAT32 User構成を決定する
- [x] ownership、update、rollback、first-boot provisioning contractを文書化する

## Implementation audit and release blockers

- [x] build target、FE導線、runtime helper、Apps、standalone、storage/update、release準備を横断監査する
  - 2026-08-13: [実装リスト](docs/developer/implementation-status.md)を作成。97 system中88 enabled、7 required component中7 present、114 libretro core、standalone 3 built / 7 pending、公開済み未実装のrelease blocker 22件を記録。
- [x] FE catalogと生成app-layerの不整合を検出する自動監査を追加する
  - `./scripts/docker-build.sh audit`は開発中のreport、`audit --release-gate`は公開済み未実装があれば失敗する。`release-image`へrelease gateを統合。
- [ ] P0 user surface blockerを0件にする
  - [x] `plumos-time-sync`とRK817 RTC/timezone/manual-time contractを実装する
    - 2026-08-13: RK817 `/dev/rtc0`へのUTC保存、8秒上限のRFC868同期、automatic/manual time、status/logをfrontend componentへ統合。実機RTC read/writeと再起動後保持は未検証。
  - [x] `plumos-storage-health`のbounded read-only FAT32検査を実装する
    - 2026-08-13: Pixel2 user volume `/mnt/plumos-user`を対象に、同梱`fsck.fat -n`、45秒上限、dirty bit/status/logを実装。RW mount中はLinuxがclean-shutdown bitをclearするため`mounted-rw`（判定保留）とし、誤ってrepair警告を出さない。RO状態での実機clean/dirty判定は未検証。
  - [x] `factory-defaults/{ra,pico,sa}` ABIと`plumos-factory-reset`を実装する
    - 2026-08-13: app-layer assemblerがRA/PicoArch/PPSSPP/DraSticのmutable path用overlayを生成し、対象別backup/atomic restore/dry-run helperを同梱。実機復元は未検証。
  - [ ] signed update backend完成までSystem Updateをplaceholderではない開発状態表示・実装gateへ接続する
  - [x] Audio OutputのSpeaker/Headphone選択をPixel2 hardware capabilityと一致させる
    - Pixel2はRK817 speaker単一路のため、存在しない出力切替をFEに表示しない。
  - [x] lidのないPixel2でLid Suspendを選択不能にする
  - [x] FTP/SFTP/SambaをPixel2 service capabilityと一致させる
    - Pixel2 imageが所有するSSH/ADBだけを表示する。未搭載daemonを機能するtoggleとして公開しない。
  - [ ] ADBを認証または明示opt-inにし、UI設定・boot・recoveryを一貫させる
  - [ ] `plumos-thumbnail-scraper`、scraper sources、plan/fetch/result導線を実装する
  - [x] `plumos-sdcard-cleanup`をPixel2 storage contractへ実装する
    - 2026-08-13: `/mnt/plumos-user/{roms,images}`を既定scopeとし、macOS/Windows sidecarだけを削除するbounded lock/interval/dry-run/cache-invalidation helperを追加。host fixtureでdry-runと削除を検証。
  - [x] ch/pt/fr/de translationを同梱し、6言語のkeyを検証する
    - 2026-08-13: en/jaを含む6言語すべてが364 keyで一致し、Pixel2以外のdevice/distro identityがないことをhost検証。実機glyph/折返しは未検証。
  - [x] arduboy、megaduck、puzzlescript、superbroswarのtheme logoを追加する
    - 2026-08-13: 190x156 RGBのPixel2 theme badgeを再現可能なgeneratorから生成し、frontend component/checksumへ統合。
  - [ ] 公開中の`standalone:pcsx_rearmed`と`standalone:yabasanshiro`を実装・実機検証する
- [ ] shared plumOS AppsをPixel2 componentとして実装する
  - [ ] Scraping
  - [ ] File Manager / NextCommander
  - [ ] Music Player
  - [ ] RetroArch menu
  - [ ] PortMaster
  - [ ] Update PortMaster
- [ ] GitHub release readinessを実装する
  - [x] top-level English READMEを追加し、日本語READMEを現行boot/image構成へ同期する
  - [ ] top-level project licenseを決定・追加する
  - [ ] third-party noticeとDraStic redistribution条件をrelease payload単位で監査する
  - [ ] CIへhost tests、identity/content gate、implementation audit、checksum検証を追加する
  - [ ] versioned artifact、SHA256SUMS、archive検査、GitHub再download検証を実装する

## Build system and app layer

- [x] Pixel2共通Docker entry pointとcomponent targetを実装する
- [x] frontendをSystemからapp-layer componentへ分離する
- [x] `plumos-text-ui`とPixel2 launcher lifecycleを統合する
- [x] Pixel2向けRetroArchをpinned sourceからbuildする
- [x] QuickNESをpinned sourceからbuildしcomponent manifest/checksumを生成する
- [x] Pixel2向けALSA `plumos_output` audio-router componentを実装する
- [x] strict app-layer assemblerとmanaged/mutable path gateを実装する
- [x] app-layerをseedしたext4 filesystemをSD image buildへ統合する
- [x] canonical libretro core recipe catalogとfilter buildを実装する
- [x] baseline core（NES/GB/GBC/SFC/MD/GBA/PCE）をbuild・route化する
  - 2026-08-12: `./scripts/docker-build.sh cores --filter plumos --jobs 4 --fail-on-error 1` で41 coreがbuild成功、component manifest/checksumを生成。
- [x] canonical all-core libretro buildを完走させる
  - 2026-08-12: `./scripts/docker-build.sh cores --filter all --jobs 4 --fail-on-error 1` で114 coreがbuild成功、component manifest/checksumを生成。
  - 2026-08-12: MF/V90S同様に並列catalog wrapper `./scripts/docker-build.sh core-catalog --filter all --concurrency 4` を追加し、114 coreをper-core cache付きでaggregate。`built=114`、`failed=0`、DeSmuME系の混入なしを確認。
- [x] PicoArch componentをPixel2 build/app-layerへ統合する
  - 2026-08-12: `./scripts/docker-build.sh picoarch` でPixel2 PicoArch/SDL12 compatをbuildし、component manifest/checksumを生成。
- [x] standalone launcher componentをPixel2 build/app-layerへ統合する
  - 2026-08-12: `./scripts/docker-build.sh standalone` でPixel2 standalone launcher componentを生成。個別standalone emulator binaryはmanifest上で`pending-binary`として明示。
- [x] Pyxel runtimeをPixel2 build/app-layer/FE導線へ統合する
  - 2026-08-13: `./scripts/docker-build.sh pyxel-runtime` でPixel2向けPython 3.11、Pyxel、pygame、numpy、Pillow、SDL2 KMSDRM/GLES依存、display-fit shim、`plumos-pyxel-pixel2-launch`、`Pyxel Setup`をcomponent manifest/checksum付きで生成。`verify-app-layer.sh`とROM route validationは`pyxel:pixel2`のlauncher/runtime欠落を失敗扱いにする。
- [ ] standalone emulator binaryをPixel2向けに順次build・実機検証する
  - 2026-08-12: OpenBOR standaloneをPixel2向けにsource buildし、`standalone/openbor/bin/OpenBOR`、runtime deps、license、component checksumへ統合。ROM set route validationでopenborは`ok`へ移行。実機起動・入力・画面向き・終了hotkeyは未検証。
  - 2026-08-12: Saturnは実装済み`retroarch:yabasanshiro`をdefault routeにし、FEから未実装standaloneを選ばない構成へ変更。standalone YabaSanshiro binary自体は未実装のまま別途検証対象。
  - 2026-08-12: Nintendo DSはPixel2向けDraStic standaloneを追加。armhf DraStic core、source-built Pixel2 integration library、package-local armhf runtime、armhf ALSA `plumos_output` pluginをapp-layerへ統合し、FE routeを`standalone:drastic`へ固定。DraStic BIOSは配布物へ含めず、実機では`/mnt/plumos-user/bios/drastic`、`/mnt/plumos-user/bios/nds`、`/mnt/plumos-user/bios`からmutable workdirへ取り込む。
  - 2026-08-12: PPSSPP v1.20.4をpinned source buildし、Pixel2向けSDL2/GLES/EGL binary、assets、factory `ppsspp.ini`/`controls.ini`、manifest/checksumへ統合。ROM set route validationでPSP `standalone:ppsspp`は`ok`へ移行し、代表ROMがある29 systemのpending binaryは0。実機での画面向き・入力・音声・終了hotkeyは未検証。
  - [ ] PCSX-ReARMed
  - [ ] YabaSanshiro
  - [ ] ScummVM
  - [ ] EasyRPG
  - [ ] Flycast
  - [ ] Mupen64Plus
  - [ ] NXEngine-Evo
- [x] Docker runtime復旧後、clean commitからPicoArch/standalone統合済みapp-layerでSD imageを再生成する
  - 2026-08-12: 4 GiB seed layout（p1=512 MiB, p2=2048 MiB, p3=remainder）でdirty-tree生成は完了し、host checksum/MBR/hdiutil partition recognitionは確認済みだったが、Docker Desktopがcontainer metadata I/O error後にAPI socketを失ったため、clean commit source_refでの再生成が必要になった。
  - 2026-08-12: Docker Desktopをforce stop/startで復旧し、clean commit `631c30b` から `./scripts/docker-build.sh sd-image` を再実行。`verify-sd-image.sh` のSYSTEM/app-layer/filesystem検証まで `sd_image=result-ok`、host checksumもOK。

## Frontend game lifecycle

- [x] FE catalogは存在するruntime/coreだけを公開する
- [x] NES ROM scan -> `retroarch:quicknes` launch planをhost検証する
- [x] Pixel2 live launcherで`PLUMOS_ROM_ROOT=/roms`からNESがRetroArchへ到達することをADB検証する
- [x] Pixel2 RetroArchが`pixel2_joypad`をport 1へautoconfigすることをADB検証する
- [ ] FEがDRM/inputを解放し、emulator終了後に再取得することを実機確認する
- [ ] Pixel2 RetroArch video rotation/scalingとframe pacingを実機確認する
- [ ] `plumos_output`経由のaudio、D-pad、ABXY、START/SELECT、shoulder、終了hotkeyを実機確認する
- [ ] save/stateが再起動後も保持されることを実機確認する
- [ ] enabled 88 systemのcontent policyを確定する
  - [ ] arcade ROM set policy: arcade、cps1、cps2、cps3、fbneo、neogeo
  - [ ] disk image policy: amiga、atari800、atarist、c64、cpc、pc88、pc98、sharpx1、thomson、vic20、x68000、zx81、zxspectrum
  - [ ] data layout policy: cannonball、cavestory、chailove、dinothawr、lowresnx、lutro、microw8、quake、wolf3d
  - [ ] frontend policy: j2me、music、ti83、vmu
  - [ ] scraper source policy: uzebox
- [ ] disabled 9 system（mame2003plus、ports、2048、bk、daphne、flashback、mrboom、palm、rickdangerous）を実装または非対応理由確定する

## Final partition and update contract

- [x] p1を512 MiB System A/B layoutへ変更する
- [x] p2を2048 MiB seed ext4 `PLUMOS_SYS`として生成する
- [ ] first bootでp2を8192 MiBへ拡張しp3 `PLUMOS_USER`を作る
- [ ] provisioningを中断・再開可能かつ既存p3非破壊にする
- [ ] initramfsへSystem A/B選択、SHA-256検証、rollbackを実装する
- [ ] frontend renderer-readyによるSystem/Runtime health promotionを実装する
- [ ] journaled Runtime updaterと1世代rollbackを実装する
- [ ] inactive-slot System updaterとreadback検証を実装する
- [ ] Ed25519署名package builder/verifierと公開鍵を実装する
- [ ] FE System Update画面とsafe reboot flowを統合する
- [ ] compact seed imageとfirst-boot後partitionをhost/実機検証する

## Boot artifact boundary

- [x] stock SDのパーティション、kernel、DTB、initramfsを読み取り専用で解析する
- [x] stock userspaceを廃止し、保持するboot artifactの境界を決定する
- [x] SD先頭16 MiBのRockchip boot領域を管理者権限で読み取り採取する
- [ ] ext4 `/storage` のfilesystem label、UUID、初回resize markerを確認する
- [x] boot artifactのprovenance、hash、サイズをmanifest化する
- [x] stock内蔵initramfsをboot substrateとして許容し、`SYSTEM`内init以降をplumOS所有にする方針を決定する
- [x] release imageをstock `Image`/stock DTB/stock kernel ABIへ戻す
- [x] stock initramfsの`SYSTEM` handoff contractをhostで再確認する
- [x] stock initramfsの`SYSTEM` handoff contractを実機で再確認する
- [x] stock initramfs hookとplumOS init早期logを実機SDから回収する
- [ ] Linux 6.12 plumOS-owned kernel経路をexperimental扱いへ隔離する
- [x] 充電中rebootがstock boot substrate + plumOS SYSTEMでOSへ戻ることを実機確認する
- [x] Pixel2のRK817 DEV_OFF経路で画面消灯することを実機確認する
- [ ] FEメニュー経由のreboot/shutdownをapp-layer checksum込みで実機確認する
  - 2026-08-12: `c875dd8` app-layerでFE経由rebootは実機確認済み。shutdownはFE経由dry-runとRK817 helper実動作を確認済み、FE経由actual poweroffのみ未実行。

## plumOS System

- [x] plumOS Pixel2 rootfsを再現可能に生成する
- [ ] stock initramfsからhandoffされるplumOS `SYSTEM`としてrootfsを生成する
- [x] stock kernel 5.10.198 ABI向けmodule/firmware/runtime manifestへ切り替える
- [x] initでstateとROMをmountする
- [x] stock由来名称・unit・frontendがSystemへ混入しないgateを実装する
- [x] Pixel2 kernel moduleと最小USB Wi-Fi firmwareをSystemへ統合する

## Frontend

- [x] 参照frontendをPixel2専用としてvendor化し、他機種・旧distribution名称を除去する
- [x] Pixel2 framebufferとgpio-key inputを自動選択してboot時にfrontendを起動する
- [x] frontendとADBの診断logをSTATE partitionへ保存する
- [x] 実機LCDでfrontend描画と90度回転を確認する
- [x] 実機でfrontendのbutton mappingを確認する
- [x] plumOS共通型のグローバル音量キー・SELECT+音量輝度サービスを実装し、自動起動と実ゲーム音量変化を確認する
  - 2026-08-12: `380a006` app-layer/SYSTEMでdaemon起動、`pixel2_joypad`/`gpio-keys` open、helper往復確認済み。SELECT+音量による画面輝度変更は実機確認済み。音量の実音声確認はゲーム起動後に実施する。
  - 2026-08-12: `c8150cc` app-layerでPixel2 audio-routerがRK817内部ルートにもruntime software gainを適用するようにした。NES起動中に音量ボタンで実音量が変化することを確認済み。

## Connectivity

- [x] USB FunctionFS/configfs ADBをbring-up時の既定保守経路にする
- [ ] release imageではADB認証または明示opt-inを必須にする
- [x] USB Wi-Fi dongle検出とwpa_supplicant経路を実装する
- [x] ADB列挙とshellを実機検証する
- [ ] USB Wi-FiとSSHを実機検証する

## Image and hardware validation

- [x] MBR、Rockchip boot領域、`PLUMOS_BOOT`、`PLUMOS_SYS`、`PLUMOS_USER`を生成する
- [x] image内のpartition境界、hash、SquashFS内容をhost検証する
- [x] 同一source refから生成したSD imageのSHA-256再現性をhost検証する
- [ ] 複製SDでcold boot、LCD、input、audio、powerを実機検証する
- [ ] app-layer manifest/checksumを実機deploy単位で検証する
- [ ] `/Volumes/public-1/02/motoki/emu/ROM/rom2`の代表ROMで全systemの実機起動・終了を検証する
  - 2026-08-12: PPSSPP統合後のhost route validationは代表ROMがある29 system中29 routeが`ok`、pending binaryは0。実機での全system起動・終了は未実施。
  - 2026-08-13: Pyxel統合後のhost route validationは代表ROMがある30 system中30 routeが`ok`、pending binaryは0。Pyxelを含む全systemの実機起動・終了は未実施。
- [ ] fb0に残るstock/旧boot splash由来の残像をclearし、実機スクショ経路をplumOS化する
