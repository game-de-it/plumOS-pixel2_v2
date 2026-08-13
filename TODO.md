# TODO

## Adopted architecture

- [x] 既存plumOS機のupdate/storage/frontend/emulator設計を調査する
- [x] Pixel2のRockchip prefix + System A/B + ext4 Runtime + FAT32 User構成を決定する
- [x] ownership、update、rollback、first-boot provisioning contractを文書化する

## Implementation audit and release blockers

- [x] build target、FE導線、runtime helper、Apps、standalone、storage/update、release準備を横断監査する
  - 2026-08-13: [実装リスト](docs/developer/implementation-status.md)を更新。SaturnをRK3326性能要件で非対応化し、97 system中87 enabled、109 libretro core、standalone 4 built / 4 pendingを現行baselineとする。
- [x] FE catalogと生成app-layerの不整合を検出する自動監査を追加する
  - `./scripts/docker-build.sh audit`は開発中のreport、`audit --release-gate`は公開済み未実装があれば失敗する。`release-image`へrelease gateを統合。
- [ ] P0 user surface blockerを0件にする
  - [x] `plumos-time-sync`とRK817 RTC/timezone/manual-time contractを実装する
    - 2026-08-13: RK817 `/dev/rtc0`へのUTC保存、8秒上限のRFC868同期、automatic/manual time、status/logをfrontend componentへ統合。実機RTC read/writeと再起動後保持は未検証。
  - [x] `plumos-storage-health`のbounded read-only FAT32検査を実装する
    - 2026-08-13: Pixel2 user volume `/mnt/plumos-user`を対象に、同梱`fsck.fat -n`、45秒上限、dirty bit/status/logを実装。RW mount中はLinuxがclean-shutdown bitをclearするため`mounted-rw`（判定保留）とし、誤ってrepair警告を出さない。RO状態での実機clean/dirty判定は未検証。
  - [x] `factory-defaults/{ra,pico,sa}` ABIと`plumos-factory-reset`を実装する
    - 2026-08-13: app-layer assemblerがRA/PicoArch/PPSSPP/DraSticのmutable path用overlayを生成し、対象別backup/atomic restore/dry-run helperを同梱。実機復元は未検証。
  - [x] signed Runtime/System updaterをFE System Updateとsafe rebootへ接続する
    - 2026-08-13: 既存plumOSの署名・journal・1世代rollback設計をPixel2の`/flash/system-slots`とnamed ABIへ移植。署名なし拒否、Runtime適用/health/rollback、中断journal復旧、inactive System readback、誤slot昇格拒否をhost fixtureで検証。
    - 2026-08-13: Ed25519署名済みRuntimeとSystemを実機適用。Runtimeはrenderer-ready前`pending_health`、ready後`healthy`、3450 checksum合格、1世代backupと設定保持を確認。Systemはinactive Bへのreadback、自動2段reboot、ready前の未昇格、B昇格、充電/USB接続中のB active再起動とFE/ADB復帰を確認。FEメニュー項目そのものの物理操作とfailure rollback injectionは未検証。
  - [x] Audio OutputのSpeaker/Headphone選択をPixel2 hardware capabilityと一致させる
    - Pixel2はRK817 speaker単一路のため、存在しない出力切替をFEに表示しない。
  - [x] lidのないPixel2でLid Suspendを選択不能にする
  - [x] FTP/SFTP/SambaをPixel2 service capabilityと一致させる
    - Pixel2 imageが所有するSSH/ADBだけを表示する。未搭載daemonを機能するtoggleとして公開しない。
  - [x] ADBを明示opt-inにし、UI設定・boot・recoveryを一貫させる
    - 2026-08-13: release defaultはOFF、`adb_enabled=1`またはFAT32 rootの`plumos-enable-adb`だけがFunctionFS gadgetを起動する。FE toggleは再起動後に反映し、OFF時はrecovery markerも除去。`adb_enabled=1`を保持したSystem A/B cold bootでFunctionFS configured、ADB shell復帰を実機確認。default OFF/recovery markerの実機検証は未完了。
  - [x] `plumos-thumbnail-scraper`、scraper sources、plan/fetch/result導線を実装する
    - 2026-08-13: MFの実績あるrunnerをPixel2のmulti-line catalogと`/mnt/plumos-user`へ適合。owned curl/dependency/CA bundle、AppsのScraping導線、atomic PNG、cache、bounded fetchを統合し、ROM setのNES 1本でCRC match/download成功をhost検証。
  - [x] `plumos-sdcard-cleanup`をPixel2 storage contractへ実装する
    - 2026-08-13: `/mnt/plumos-user/{roms,images}`を既定scopeとし、macOS/Windows sidecarだけを削除するbounded lock/interval/dry-run/cache-invalidation helperを追加。host fixtureでdry-runと削除を検証。
  - [x] ch/pt/fr/de translationを同梱し、6言語のkeyを検証する
    - 2026-08-13: en/jaを含む6言語すべてが364 keyで一致し、Pixel2以外のdevice/distro identityがないことをhost検証。実機glyph/折返しは未検証。
  - [x] arduboy、megaduck、puzzlescript、superbroswarのtheme logoを追加する
    - 2026-08-13: 190x156 RGBのPixel2 theme badgeを再現可能なgeneratorから生成し、frontend component/checksumへ統合。
  - [ ] 公開中の`standalone:pcsx_rearmed`を実装・実機検証する
    - [x] PCSX-ReARMed r26l、sdl12-compat、Pixel2回転fbdev、入力、48 kHz音声、factory configを再現可能にhost buildする
    - [ ] PCSX-ReARMedを実機で起動し、画面、全入力、音声、menu/exit、FE復帰を確認する
      - 2026-08-13: 実機でFunction menuが開かないことを再現。SDL番号依存を廃止し、raw `BTN_TRIGGER_HAPPY1`のlibpicofe evdev menuへ修正。build/deploy後の実機再確認が必要。
      - 2026-08-13: PCSX menu中のraw captureで十字が`BTN_DPAD_UP/DOWN`（544/545）、A/Bが305/304と確定。`BTN_DPAD_*`のgame/menu bindを追加した`3234b0d`を署名Runtimeで適用し、Function menuの十字移動・A決定・B戻るを実機確認。音声、menuからのexit、FE復帰、second launch、save保持は継続。
    - [x] Saturn/YabaSanshiroをRK3326性能要件により非対応化し、FE導線・core recipe・standalone manifestから除外する
      - 2026-08-13: `b66f3c8`で`unsupported_performance_rk3326`を明示し、Saturn 2 coreと全routeを削除。
- [ ] shared plumOS AppsをPixel2 componentとして実装する
  - [x] Scraping
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
  - 2026-08-13: Saturn 2 core、実機で全構成segfaultしたMupen64Plus-Next、互換ROM revisionで起動しなかったCPS1/CPS2専用FBA2012重複coreを対象外にし、現行109 coreを並列catalog wrapper `./scripts/docker-build.sh core-catalog --filter all --concurrency 4`で再生成。109成功・0失敗を確認。
- [x] PicoArch componentをPixel2 build/app-layerへ統合する
  - 2026-08-12: `./scripts/docker-build.sh picoarch` でPixel2 PicoArch/SDL12 compatをbuildし、component manifest/checksumを生成。
- [x] standalone launcher componentをPixel2 build/app-layerへ統合する
  - 2026-08-12: `./scripts/docker-build.sh standalone` でPixel2 standalone launcher componentを生成。個別standalone emulator binaryはmanifest上で`pending-binary`として明示。
- [x] Pyxel runtimeをPixel2 build/app-layer/FE導線へ統合する
  - 2026-08-13: `./scripts/docker-build.sh pyxel-runtime` でPixel2向けPython 3.11、Pyxel、pygame、numpy、Pillow、SDL2 KMSDRM/GLES依存、display-fit shim、`plumos-pyxel-pixel2-launch`、`Pyxel Setup`をcomponent manifest/checksum付きで生成。`verify-app-layer.sh`とROM route validationは`pyxel:pixel2`のlauncher/runtime欠落を失敗扱いにする。
- [ ] standalone emulator binaryをPixel2向けに順次build・実機検証する
  - 2026-08-12: OpenBOR standaloneをPixel2向けにsource buildし、`standalone/openbor/bin/OpenBOR`、runtime deps、license、component checksumへ統合。ROM set route validationでopenborは`ok`へ移行。実機起動・入力・画面向き・終了hotkeyは未検証。
  - 2026-08-13: N64のMupen64Plus-Nextはdynarec無効、cached/pure interpreter、GLideN64/Angrylion/ParaLLElの全実機試験でsegfaultしたため、壊れたalternateを残さずParallel N64だけを公開する。
  - 2026-08-12: Nintendo DSはPixel2向けDraStic standaloneを追加。armhf DraStic core、source-built Pixel2 integration library、package-local armhf runtime、armhf ALSA `plumos_output` pluginをapp-layerへ統合し、FE routeを`standalone:drastic`へ固定。DraStic BIOSは配布物へ含めず、実機では`/mnt/plumos-user/bios/drastic`、`/mnt/plumos-user/bios/nds`、`/mnt/plumos-user/bios`からmutable workdirへ取り込む。
  - 2026-08-12: PPSSPP v1.20.4をpinned source buildし、Pixel2向けSDL2/GLES/EGL binary、assets、factory `ppsspp.ini`/`controls.ini`、manifest/checksumへ統合。ROM set route validationでPSP `standalone:ppsspp`は`ok`へ移行し、代表ROMがある29 systemのpending binaryは0。実機での画面向き・入力・音声・終了hotkeyは未検証。
  - 2026-08-14: PPSSPPの縦画面とFunction無反応を実機再現。Pixel2の480x640 panelを論理640x480からCCW回転するpresenterと、実測した15-button mapping（Function=SDL Guide button 14）をsource buildへ統合。`612822f`でPSP比率補正0.5625とUI scale -2へ移行し、署名Runtime `0.1.0-dev-612822f`のhealth昇格、DRM実画面640x363、物理Functionでの拡大pause menu表示を確認。オペレータも画面表示と文字サイズを合格判定。
  - 2026-08-14: `Telegraph Crosswords.cso`のDRM primary planeを直接captureし、ゲーム領域が640x204（約3.14:1）へ潰れることを確認。同じportrait panelのA30で実績あるLandscape補正`0.562500`により640x363（PSP native約1.76:1）へ修正し、UI scaleも`-8`から`-2`へ拡大。正式Runtime適用後のゲーム画面とpause menuのDRM capture、物理Function操作、FE復帰まで合格。
  - [ ] PCSX-ReARMed
    - [x] pinned sourceからAArch64 binaryとpackage-local sdl12-compatをbuildし、component manifest/checksumへ統合する
    - [x] Pixel2の480x640 framebufferへCCW回転した論理640x480/4:3 presenter、物理button/hat、`plumos_output` 48 kHz routeを実装する
    - [x] clean `8b54b97` app-layer、署名Runtime差分、実機health promotion、3468 root checksumを検証する
    - [x] ROM setの`PSX/chroQW.img`を実機へSHA一致で配置し、`standalone:pcsx_rearmed` launch planを解決する
    - [ ] 代表PSX ROMで実機acceptanceを完了する
  - [ ] ScummVM
  - [ ] EasyRPG
  - [ ] Flycast
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
- [ ] RA/PicoArch/SAの物理Function menuを実機確認する
  - 2026-08-13: RA、PicoArch、PCSX-ReARMed、DraStic、PPSSPP、OpenBORのFunction menu契約を実装し、source contract testを追加。`e9c8f38`から全対象をbuildし、署名Runtimeを実機へ適用。health昇格、対象22 SHA一致、root checksum 3470件合格。各runtimeの物理menu/exit確認が必要。
  - 2026-08-13: PCSX内蔵menuでevdevとSDL joystickが同じ`event2`を二重登録する状態を実機FDで確認。Pixel2のPCSXはraw evdevだけをcontroller入力元とする`002e250`へ修正し、全4 SAを並列build、署名Runtime `0.1.0-dev-002e250`を適用。health昇格、実機root checksum 3470件/失敗0。PCSX menuの十字/A決定/B戻るは物理再確認待ち。
  - 2026-08-13: `3234b0d`でPCSX menuの物理Function、十字、A決定、B戻るを実機合格。RA、PicoArch、DraStic、PPSSPP、OpenBORは引き続き個別物理確認が必要。
- [ ] Pixel2 RetroArch video rotation/scalingとframe pacingを実機確認する
- [ ] `plumos_output`経由のaudio、D-pad、ABXY、START/SELECT、shoulder、終了hotkeyを実機確認する
- [ ] save/stateが再起動後も保持されることを実機確認する
- [ ] enabled systemのBIOS/firmware inventoryを完備する
  - [x] 有効routeのlibretro `.info`とstandalone要求からPixel2 BIOS staging/manifestを生成する
  - [ ] ROMセットに無い必須firmwareを補完し、missing requiredを0件にする
  - [x] ROMセットに存在するBlueMSX archiveを含む486 firmware fileを実機`/mnt/plumos-user/bios`へmerge-only配置する
  - [ ] enabled systemごとにBIOS検出と代表content起動を実機確認する
- [ ] enabled 87 systemのcontent policyを確定する
  - [ ] arcade ROM set policy: arcade、cps1、cps2、cps3、fbneo、neogeo
  - [ ] disk image policy: amiga、atari800、atarist、c64、cpc、pc88、pc98、sharpx1、thomson、vic20、x68000、zx81、zxspectrum
  - [ ] data layout policy: cannonball、cavestory、chailove、dinothawr、lowresnx、lutro、microw8、quake、wolf3d
  - [ ] frontend policy: j2me、music、ti83、vmu
  - [ ] scraper source policy: uzebox
- [ ] disabled 10 system（saturn、mame2003plus、ports、2048、bk、daphne、flashback、mrboom、palm、rickdangerous）を実装または非対応理由確定する
  - [x] Saturnは`unsupported_performance_rk3326`として非対応理由を確定する

## Final partition and update contract

- [x] p1を512 MiB System A/B layoutへ変更する
- [x] p2を2048 MiB seed ext4 `PLUMOS_SYS`として生成する
- [ ] first bootでp2を8192 MiBへ拡張しp3 `PLUMOS_USER`を作る
- [ ] provisioningを中断・再開可能かつ既存p3非破壊にする
- [x] stock initramfs固定handoffの内側へSystem A/B選択、SHA-256検証、rollbackを実装する
  - 2026-08-13: stockが固定で開く`/SYSTEM`を小さなPixel2 dispatcherとし、FAT32で名前衝突しない`/system-slots/system-{a,b}.squashfs`を選択する。pendingは一度だけ試し、次bootまでFE health promotionがなければactiveへrollback。`5932ef9`でslot A cold boot、`7f16e6d`でinactive B pending boot、FE promotion、B active再起動、mount継承、旧root detach、FE/ADB起動を実機確認。実機failure rollbackは未検証。
- [x] frontend renderer-readyによるSystem health promotionを実装する
  - 2026-08-13: FE自身が初回描画成功後に作る`/tmp/plumos-fe-ready`だけをproofとし、dispatcherが記録した`system-booted`とpending slotが一致する場合だけactiveへatomic promotionする。slot B実機bootでready前は`active=a,pending=b,attempted=b`を保持し、ready後だけ`active=b`へ昇格してpending/attemptedを消去。B active再起動でも保持を確認。Runtime promotionはtransactional updaterと同時に実装する。
- [x] journaled Runtime updaterと1世代rollbackを実装する
- [x] inactive-slot System updaterとreadback検証を実装する
- [x] Ed25519署名package builder/verifierと公開鍵を実装する
- [x] FE System Update画面とsafe reboot flowを統合する
  - 2026-08-13: `tests/test-pixel2-update.sh`とARM64 System chroot検証は合格。公開鍵だけをSystemへ同梱し秘密鍵混入gateを追加。ADBからrequestしたsigned Runtime/Systemの実機成功経路は合格。FEからのrequest、進捗/失敗表示、実機failure rollbackはacceptanceとして継続する。
- [ ] compact seed imageとfirst-boot後partitionをhost/実機検証する
  - 2026-08-14: 現行64 GB実機SDで最終境界（p2=8192 MiB、p3=残り49.7 GiB）、既存user data復元、cold boot mountには合格。release seedから同じ処理を中断再開可能に行うfirst-boot provisionerは未実装のため項目は継続。

## Boot artifact boundary

- [x] stock initramfsのIUX boot logoをplumOSへ置換する
  - 2026-08-14: `load_splash()`が`mount_flash()`より先に実行されるため、boot FATのOEM画像だけでは置換できないことを実機timestampで確定。stock Image/DTBを維持し、`post-flash.sh`がinitramfsの`ply-image`でマウント直後にplumOS画像を再描画する方式へ修正。IUXが一瞬表示された後に正しいplumOSロゴへ切り替わることを実機LCDで確認。初期IUXも完全に除去する場合だけstock Image内蔵initramfs画像の再packが必要。
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
- [x] stock initramfsからhandoffされるplumOS `SYSTEM`としてrootfsを生成する
- [x] stock kernel 5.10.198 ABI向けmodule/firmware/runtime manifestへ切り替える
- [x] initでstateとROMをmountする
- [x] stock由来名称・unit・frontendがSystemへ混入しないgateを実装する
- [x] Pixel2 kernel moduleと最小USB Wi-Fi firmwareをSystemへ統合する

## Frontend

- [x] Pixel2の標準system pickerをV90S共通の3x2・6アイコンgridに揃える
  - 2026-08-14: 初期移植時の`default-horizontal` / `tile_strip`（2x1）を廃止し、標準`default` themeを`tile_grid`（3x2）、vertical page transitionへ変更。`c808952`の署名Runtime差分を実機適用し、3490 checksum、`runtime_healthy`、設定保持を確認。LCD上の6 tileと物理navigationは目視確認待ち。
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
- [x] release imageではADB認証または明示opt-inを必須にする
- [x] USB Wi-Fi dongle検出とwpa_supplicant経路を実装する
- [x] ADB列挙とshellを実機検証する
- [ ] USB Wi-FiとSSHを実機検証する

## Image and hardware validation

- [x] MBR、Rockchip boot領域、`PLUMOS_BOOT`、`PLUMOS_SYS`、`PLUMOS_USER`を生成する
- [x] image内のpartition境界、hash、SquashFS内容をhost検証する
- [x] 同一source refから生成したSD imageのSHA-256再現性をhost検証する
- [x] 通常起動のruntime integrity gateを高速化する
  - 2026-08-14: [実機boot profile](docs/validation/2026-08-14-pixel2-boot-profile.md)でkernel開始からFE renderer-readyまで約67.3秒を計測。1.2 GiB・3490 fileのroot checksumが52.60秒（約78%）、request無しPython updaterがcold時4.30秒、System slot checksumが1.16秒、ROM scanは174 msだった。
  - 2026-08-14: `25d1af5`で通常bootのfull hashとidle Python updaterを除外し、`46fb284`で明示`verify-runtime`をBusyBox対応。署名System A/B更新、slot readback、health昇格、active slot再起動に合格。最終通常bootはfrontend process開始8.28秒、renderer-readyは8.67秒以内、ADB 6.97秒、ROM scan 162 ms。完全3490 checksumは更新前と明示保守時だけ合格確認した。
- [ ] 複製SDでcold boot、LCD、input、audio、powerを実機検証する
- [x] app-layer manifest/checksumを実機deploy単位で検証する
  - 2026-08-13: A/B slot A起動後、`checksums.sha256`の管理対象3450件が全て一致し、FEも`app-layer-verified`から起動した。
- [ ] `/Volumes/public-1/02/motoki/emu/ROM/rom2`の代表ROMで全systemの実機起動・終了を検証する
  - 2026-08-12: PPSSPP統合後のhost route validationは代表ROMがある29 system中29 routeが`ok`、pending binaryは0。実機での全system起動・終了は未実施。
  - 2026-08-13: Pyxel統合後のhost route validationは代表ROMがある30 system中30 routeが`ok`、pending binaryは0。Pyxelを含む全systemの実機起動・終了は未実施。
  - 2026-08-13: Saturn廃止後は87 enabled、109 libretro core、standalone 4 built / 4 pending。ROM setのトップレベル、`_etc`、共有ATARI/MAME directoryを探索し、互換contentがある74 system・165 profileを抽出した。
  - [x] ROMセットに代表contentがある29 system・97 profileを実機で3秒起動し、97/97 early-start passを記録する
  - [x] archival/shared/directory-backed contentを追加検出し、現行routeで73 system・164 profileのearly-start passを記録する
  - [x] early-start passに使用した73 system・78代表セットを通常FEディレクトリへ復元し、目視確認へ引き継ぐ
    - 2026-08-14: 一時smoke contentの自動削除だけで終了していた手順を是正。p2=8 GiB、p3=残り49.7 GiBへ実機SDを拡張し、旧p3の651 fileを651/651 SHA一致で復元。pass report由来1,447 source file + 2 markerを恒久配置し、再適用で0 transfer / 1,447 identical skip、cold boot後73 system・83 FE entry、frontend readyを確認。
  - [ ] Channel Fの必須BIOS `sl31253.bin`、`sl31254.bin`、`sl90025.bin`を正規に用意し、残る1 profileを起動確認する
  - [ ] ROMセットにmatching contentが無い13 enabled systemへ代表contentを用意し、実機起動を記録する
    - `ngp`, `wonderswancolor`, `x68000`, `tic80`, `vectrex`, `sg1000`, `sharpx1`, `wolf3d`, `zx81`, `arduboy`, `megaduck`, `puzzlescript`, `superbroswar`
- [ ] fb0に残るstock/旧boot splash由来の残像をclearし、実機スクショ経路をplumOS化する
