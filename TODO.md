# TODO

## Adopted architecture

- [x] 既存plumOS機のupdate/storage/frontend/emulator設計を調査する
- [x] Pixel2のRockchip prefix + System A/B + ext4 Runtime + FAT32 User構成を決定する
- [x] ownership、update、rollback、first-boot provisioning contractを文書化する

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
- [x] PicoArch componentをPixel2 build/app-layerへ統合する
  - 2026-08-12: `./scripts/docker-build.sh picoarch` でPixel2 PicoArch/SDL12 compatをbuildし、component manifest/checksumを生成。
- [x] standalone launcher componentをPixel2 build/app-layerへ統合する
  - 2026-08-12: `./scripts/docker-build.sh standalone` でPixel2 standalone launcher componentを生成。個別standalone emulator binaryはmanifest上で`pending-binary`として明示。
- [ ] standalone emulator binaryをPixel2向けに順次build・実機検証する
  - 2026-08-12: OpenBOR standaloneをPixel2向けにsource buildし、`standalone/openbor/bin/OpenBOR`、runtime deps、license、component checksumへ統合。ROM set route validationでopenborは`ok`へ移行。実機起動・入力・画面向き・終了hotkeyは未検証。
  - 2026-08-12: Saturnは実装済み`retroarch:yabasanshiro`をdefault routeにし、FEから未実装standaloneを選ばない構成へ変更。standalone YabaSanshiro binary自体は未実装のまま別途検証対象。
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
- [ ] fb0に残るstock/旧boot splash由来の残像をclearし、実機スクショ経路をplumOS化する
