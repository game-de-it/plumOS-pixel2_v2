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
    - 2026-08-14: 4135-file full Runtimeで旧journal方式が12,405回・累計約2.96 GBのJSON書込みになることを検出。`164c841`で完全操作計画を1回だけpre-journalし、backupを開始済み証拠にする線形I/O rollbackへ修正。`89fa6a4`でPixel2 480x640 update進捗6画面もSystemへ収録した。旧方式で開始済み実機transactionと修正版Systemの物理acceptanceは継続。
  - [x] Audio OutputのSpeaker/Headphone選択をPixel2 hardware capabilityと一致させる
    - Pixel2はRK817 speaker単一路のため、存在しない出力切替をFEに表示しない。
  - [x] lidのないPixel2でLid Suspendを選択不能にする
  - [x] 物理Powerのglobal menuとPixel2 sleep/standbyを実装する
    - 2026-08-15: `rk805 pwrkey` (`event0`) をFEと常駐hardware-key
      serviceの双方で扱い、FE外ではdisplay ownerだけをSIGSTOPして同じpower
      menuをoverlay表示する。Sleep/Reboot/Shutdown/Cancelを共通化し、terminal
      action前にはownerを再開してsave/TERM処理を許可する。
    - stock 5.10.198は`freeze mem`を公開するが、USB給電中の実機では両stateを
      suspend開始前に`EBUSY`で拒否した。kernel sleepを優先し、拒否時はdisplay
      owner停止、backlight消灯、次のPowerだけで復帰するsoftware standbyへ
      自動fallbackする。復帰後はPixel2固有のRK817 `Speaker`/`Headphone`
      switch、輝度、音量状態、必要時のADB gadgetを復元する。
    - 署名Runtime `0.1.0-dev-c5d9c16`を実機適用。FEのDRM両bufferを黒でpresent
      した後、connector DPMS OFFとCRTC切断でDSI panelを完全消灯する。USBを
      sleep中に抜き差ししても消灯を維持し、2秒後のpolicy-aware ADB再起動で
      ADBが自動復帰、物理Power 1回で輝度28・FE操作まで正常復帰することを確認。
    - 2026-08-16: Runtime `0.1.0-dev-e9a69a9`でRAのDRM masterとactive KMS
      plane 2枚をoverlayへ安全にhandoffする経路を実機確認。ゲーム中にPower
      menuを表示し、Sleep、USB電源の切断・再接続、物理Power 1回の順で同じRA
      processへ映像・入力とも復帰した。ADBも画面を起こさず自動復帰した。
      RAのCancel、およびPicoArch/SA/Appsのoverlay表示、Cancel、復帰後の
      映像・入力・音声はoperator確認を継続する。
    - 2026-08-16: DraSticはDRMを所有するAArch64 runnerと、画面FDを持たず
      emulationを進めるarmhf coreが別processだったため、runnerだけを停止する
      とsleep中もゲームが進行した。`11e6ef5`でruntime PID/exe・共通parentを
      検証して両方を一体停止/復帰し、`12b809b`でADB切断時も停止中runnerの
      cleanupが固まらないようSIGCONT後TERMとbounded waitを追加。署名Runtime
      `0.1.0-dev-12b809b`でFE起動、power menu、Cancel、Sleep、sleep中USB電源
      抜き差し、ADB再列挙、Power 1回復帰、ゲーム継続、映像・入力・音声を
      operator合格とした。DraSticのsleep gateは完了。
    - 2026-08-16: PicoArchは署名Runtime `0.1.0-dev-45b4505`でPower menuからの
      sleep/wakeと、ゲーム画面・内蔵menu双方の物理D-pad操作をoperator合格とした。
      `d1f5ea1`でPixel2固有`BTN_DPAD_*`をgame/menu bindへ追加済み。
    - 2026-08-16: Gambatteで音声だけ継続してLCDが黒くなる状態を、fbdevには有効な
      frameがある一方KMS active planeがない問題と特定。`d242dfc`で初回frameと
      `SIGCONT`後だけUNBLANK/PANし、Gambatte固有RGB565 byte swapも既存XU20の
      実績から移植した。署名Runtime `0.1.0-dev-d242dfc`はhealthy、component
      checksum合格、実機色表示と物理Function menu表示をoperator合格とした。
  - [x] FTP/SFTP/SambaをPixel2 componentとして実装する
    - 2026-08-14: 初期bring-upの「SSH/ADBだけを表示」を撤回し、V90S/MFと同じ5 serviceをpackage化。保存設定のboot再開、component checksum、release gateへ統合した。USB Wi-Fi実機でのFTP/SFTP/Samba接続は未検証。
    - 2026-08-17: Pixel2だけに残っていた鍵必須SSHを廃止し、V90S/MF共通の
      `root / plumos`初期認証、端末ローカルsalt付きshadow、公開鍵併用、永続host
      key、fresh imageのSSH既定ONへ統一。FTP/SFTP/Samba/ADBを含む実機protocol
      round-tripと再起動復元をrelease gateとして継続する。
    - 2026-08-17: minimal initがloopbackを起動しておらずADB port forwardingが
      SSHへ到達しない欠陥をtraceで特定。System initとapp-layer service managerの
      双方で`lo`と`127.0.0.1/8`を保証し、実機password SSH loginとSFTP upload/
      download同一SHA-256を確認した。
  - [x] ADBのboot既定値・UI設定・recoveryを一貫させる
    - 2026-08-14: Wi-Fi非搭載Pixel2で保守経路を失わないよう、設定未作成時だけADBを既定ONへ戻した。FEで保存した`adb_enabled=0/1`を最優先し、FAT32 rootの`plumos-enable-adb`は明示OFFからも復旧できる。新Systemの実機cold boot確認は継続。
    - 2026-08-14: ADB不能SDへ`67c25aa` System dispatcher/A/Bをoffline recoveryとして適用。旧`d56bf29`一式はFAT32 user volumeへ退避し、stock Image/DTB、Runtime、ROM、BIOS、設定を保持。cold boot ADB確認とRuntime transactional updateは継続。
    - 2026-08-16: USB給電のoffline/online transitionを常駐hardware-key serviceで
      検出する。`45b4505`では通常`recover`と物理再接続`replug`を分離し、再接続
      2秒後だけUDC rebind、失敗時は単発clean restartを行う。全mutating actionを
      PID lockで直列化し、二重adbd/JDWP/FunctionFS競合を防止した。署名System/
      Runtime `0.1.0-dev-45b4505`を実機適用し、USB抜き差し後のFunctionFS
      BIND/ENABLE、adbd 1 process、UDC configured、ADB shell復帰を確認。
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
- [x] shared plumOS AppsをPixel2 componentとして実装する
  - [x] Scraping
  - [x] File Manager / NextCommander
  - [x] Music Player
  - [x] RetroArch menu
  - [x] PortMaster
  - [x] Update PortMaster
  - [x] Ports systemからPortMaster install済みscriptを起動する導線
  - 2026-08-14: 共有7 Appsをcatalog、component manifest/checksum、visible launcher存在gateへ統合。host build済み。各Appsの物理入力・表示・音声・終了後FE復帰は実機acceptanceが必要。
  - 2026-08-15: 実機backend監査でScraping plan、File Manager、Music Player、RetroArch RGUI、Pyxel Setup、PortMasterを合格。RetroArch Appのudev準備漏れ、FE stop/launch helper欠落、zombie誤認を`85fffad`で修正。Update PortMasterのnetwork installと7 AppsのFE物理選択は継続。
  - 2026-08-15: File ManagerのMF button order誤流用と、回転rendererが論理640幅を物理480幅でclipする不具合を`9b4070d`、`0106a75`で修正。署名Runtime、全幅DRM capture、event2経由のD-pad/A/B/FUNCTION/Quit、FE再取得に合格。実物buttonのoperator目視は継続。
  - 2026-08-15: File Managerが外部commandを`execvp("cp")`等で起動する一方、Pixel2 Systemには個別の`/bin/cp`等が無く、copy/move/link/rename/delete/mkdirが失敗していた。`883fd1d`でPixel2だけBusyBox本体へapplet名を渡す実行方式に変更し、複数選択progress判定も修正。署名Runtime `0.1.0-dev-883fd1d`を適用し、実際のX/A操作によるcopy、コピー元/先SHA-256一致、FE復帰に合格。
  - 2026-08-15: Music PlayerがLinuxの`BTN_A=304`、`BTN_B=305`を物理labelとして扱い、Pixel2の物理A=305/B=304と逆転していた。FUNCTION=704も未処理だった。`b370bfa`でA=再生、B/FUNCTION=終了へ修正し、D-pad、A/B/X/Y、START/SELECT、L/R、FUNCTIONの実機EV_KEY経路、3曲遷移、音声開始、clean exit、FE復帰に合格。
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
  - 2026-08-15: `plumos-frontend-stop/launch`を追加し、RetroArch RGUI起動前の解放、終了後のFE/hardware-key daemon再取得をADBで確認。物理FEからの全emulator終了acceptanceは継続。
- [ ] RA/PicoArch/SAの物理Function menuを実機確認する
  - 2026-08-13: RA、PicoArch、PCSX-ReARMed、DraStic、PPSSPP、OpenBORのFunction menu契約を実装し、source contract testを追加。`e9c8f38`から全対象をbuildし、署名Runtimeを実機へ適用。health昇格、対象22 SHA一致、root checksum 3470件合格。各runtimeの物理menu/exit確認が必要。
  - 2026-08-13: PCSX内蔵menuでevdevとSDL joystickが同じ`event2`を二重登録する状態を実機FDで確認。Pixel2のPCSXはraw evdevだけをcontroller入力元とする`002e250`へ修正し、全4 SAを並列build、署名Runtime `0.1.0-dev-002e250`を適用。health昇格、実機root checksum 3470件/失敗0。PCSX menuの十字/A決定/B戻るは物理再確認待ち。
  - 2026-08-13: `3234b0d`でPCSX menuの物理Function、十字、A決定、B戻るを実機合格。RA、PicoArch、DraStic、PPSSPP、OpenBORは引き続き個別物理確認が必要。
  - 2026-08-16: PicoArchはFunctionから内蔵menuを表示し、`d1f5ea1`適用後に
    game/menuの十字操作とsleep復帰を実機合格。RA、DraStic、PPSSPP、OpenBORの
    個別menu/exit確認は継続する。
- [x] RetroArch factory configuration一式をPixel2へ実装する
  - 2026-08-15: 3,374 unique keyをPixel2 DRM/udev/button/audio/storage contractへ適合。旧57-key factoryは既知SHA一致時だけatomic置換し、変更済みuser cfgには不足keyだけを補完するmigrationを追加。署名Runtime `0.1.0-dev-aa3a3ab`を適用し、healthy、root 4241/4241 checksum、RGUIのDRM/event2取得、FE復帰を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-v90s-config-port.md)
  - 2026-08-15: 前記移植がmain cfgだけで、content-local save/state、L2/R2 hotkey変換、core-options、N64 remapを欠いていたことを訂正。`68abe6c`で3-file factory bundleと旧世代12項目の限定migrationを実装し、署名Runtime `0.1.0-dev-68abe6c`を適用。healthy、Frontend 191/191、RetroArch 59/59、root 4245/4245、既存state 2件のSHA不変を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-save-hotkeys.md)
  - 2026-08-15: `68abe6c`がPixel2で動作確認済みのSTART+SELECT直接終了をV90Sのmenu comboへ誤って変更した回帰を確認。`612cc19`で`enable=SELECT(8)`+`exit=START(9)`を復元し、FUNCTION menu(14)を維持。旧Runtimeの2値だけを直すmigrationを署名Runtimeへ適用し、既存fallback/content-local state 6件のSHA不変を確認。
  - 2026-08-15: plain DRM OSDが物理480x640座標へ未回転描画され、pending messageもthread間で無保護だった。`70357bb`で論理640x480からの回転描画、mutex、glyph bounds、font既定値と限定migrationを実装。署名Runtime上でsave slot 8通知の正方向・全文表示、RA継続動作、RetroArch 59/59とFrontend 191/191 checksum、試験設定のbyte一致復元を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-osd.md)
  - 2026-08-15: 物理実機で動作確認済みのmutable `retroarch.cfg`をbyte-for-byteでfactory/build defaultへ採用（3,376 unique key、SHA-256 `231ee258...`）。`72f42e5`から同versionでRetroArch、Frontend、strict app-layerを再buildし、署名Runtime `0.1.0-dev-72f42e5`を適用。active/factory/factory-reset用cfgの同一性、既存active cfg不変のmarker移行、RetroArch 59/59、Frontend 191/191、FE/ADB稼働を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-live-default.md)
- [ ] Pixel2 RetroArch video rotation/scalingとframe pacingを実機確認する
  - 2026-08-14: WonderSwan `Puzzle Bobble.ws` (`mednafen_wswan`)でSELECTによる縦/横切替後の表示が180度逆さになることをDRM overlay planeのRGB565 captureで確認。`465b957`でWonderSwan系のみ`video_allow_rotate=false`とし、core内content回転後にPixel2固定panel補正を適用。署名Runtime `0.1.0-dev-465b957`のhealth昇格と縦向き正方向captureは合格。SELECT切替後の横向き物理captureは継続。
  - 2026-08-14: 回転修正後に共通4:3固定でWonderSwan映像が伸長されることを実機確認。`e327fb9`でcore-provided aspectへ切り替えたが、SELECT後の144x224 frameへPixel2固定回転分のaspect反転が二重適用され、物理方向640x411になる誤りを目視指摘で再確認。`f7bd277`で`ASPECT_RATIO_CORE`だけDRM側の重複反転を相殺し、物理SELECT 1回後の正式Runtime captureを309x480（144:224と丸め誤差内で一致）へ修正。署名Runtime `0.1.0-dev-f7bd277`はhealth昇格済み。最終LCD目視確認は継続。
  - 2026-08-14: `5c99bd9`でWonderSwanのcontent回転とPixel2固定panel回転を分離。`video_rotation=0`、core software rotation、core-provided aspect、最終`PLUMOS_DRM_PANEL_ROTATION=3`とし、拒否したcore rotationをfrontend aspectへ残さない。物理SELECT後の144x224 / 9:14、正方向表示をDRM captureと実機目視で合格。署名Runtime `0.1.0-dev-5c99bd9`はhealth昇格、managed SHA一致、3490/3490 checksum合格。
  - 2026-08-16: DreamcastのFlycast XtremeをKMS/GBM `gl`へ移し、GL用`video_rotation=1`、Dreamcastのcore rotation拒否、Full aspectで正立640x480全面表示へ修正。続いてRGUIだけが無回転行列で描画される問題を`f46740d`でDreamcast限定のcontent行列へ修正。署名Runtime `0.1.0-dev-f46740d`はhealthy、Frontend 194/194、RetroArch 59/59、PicoArch 11/11、root 4248/4248に合格。最終DRM captureと物理Functionで開いたメニューの実LCD方向をoperator確認済み。[検証記録](docs/validation/2026-08-16-pixel2-dreamcast-display.md)
  - 2026-08-16: app-layer manifestのGLES必須4 core（Flycast 2種、ParaLLEl N64、DuckSwanStation）を監査。N64だけ`drm`へ漏れて逆さま・3:4表示だったため、`2944596`で全hardware-GLES coreを`gl`、rotation 1、Full aspect、content-matrix RGUIへ統一し、manifestに追加されたGLES coreのlauncher漏れをrelease gate化。署名Runtime `0.1.0-dev-2944596`はhealthy、Frontend 194/194、RetroArch 59/59、cores 357/357、root 4248/4248に合格。N64、DuckSwan、通常Flycastの最終DRM game captureとN64/DuckSwanのRGUI captureは正立640x480。さらに正式Runtime上で物理Functionから開いたN64メニューの実LCD方向をoperator確認し、最終XR24 plane captureも合格。[検証記録](docs/validation/2026-08-16-pixel2-gles-core-audit.md)
- [ ] `plumos_output`経由のaudio、D-pad、ABXY、START/SELECT、shoulder、終了hotkeyを実機確認する
- [ ] save/stateが再起動後も保持されることを実機確認する
  - 2026-08-15: content-local save/state、自動exit state、10秒autosave、20世代state、thumbnailをfactoryで有効化。更新前から存在したfallback state 2件は更新・再起動後もSHA一致。ゲーム内で新規save/stateを書き、再起動後にloadする物理acceptanceは継続。
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
- [ ] disabled 9 system（saturn、mame2003plus、2048、bk、daphne、flashback、mrboom、palm、rickdangerous）を実装または非対応理由確定する
  - [x] Saturnは`unsupported_performance_rk3326`として非対応理由を確定する

## Final partition and update contract

- [x] p1を512 MiB System A/B layoutへ変更する
- [x] p2を2048 MiB seed ext4 `PLUMOS_SYS`として生成する
- [x] first bootでp2を8192 MiBへ拡張しp3 `PLUMOS_USER`を作る
  - 2026-08-14: compact p1+p2 MBR seed、online `resize2fs`、残容量FAT32作成、ROM/BIOS directory seedをSystem early initへ統合。stock handoffがmount済みp2のgeometry更新を拒否した場合はsync後1回rebootして再開する。
- [x] provisioningを中断・再開可能かつ既存p3非破壊にする
  - 2026-08-14: p3 ownershipをMBR変更前にext4 journalへcommitし、各stageを再実行可能にした。16 GB sparse-card simulationで中断resume、完了後無書込み、旧4 GiB layoutのp3 byte保持を検証。
  - 2026-08-14: 実機SystemのBusyBoxに`blkid` appletが無いのに`/bin/blkid` symlinkを作っていたため既存FAT32判定が失敗し、p3を3回再formatした。`b902e3e`でutil-linux実体を収録し、boot-sector type/label fallbackとmissing-blkid非破壊fixtureを追加。修正版boot後はsentinel保持とformat count不増を確認。format前user dataを完全退避した証明はなく、data lossの可能性を記録する。
- [x] stock initramfs固定handoffの内側へSystem A/B選択、SHA-256検証、rollbackを実装する
  - 2026-08-13: stockが固定で開く`/SYSTEM`を小さなPixel2 dispatcherとし、FAT32で名前衝突しない`/system-slots/system-{a,b}.squashfs`を選択する。pendingは一度だけ試し、次bootまでFE health promotionがなければactiveへrollback。`5932ef9`でslot A cold boot、`7f16e6d`でinactive B pending boot、FE promotion、B active再起動、mount継承、旧root detach、FE/ADB起動を実機確認。実機failure rollbackは未検証。
- [x] frontend renderer-readyによるSystem health promotionを実装する
  - 2026-08-13: FE自身が初回描画成功後に作る`/tmp/plumos-fe-ready`だけをproofとし、dispatcherが記録した`system-booted`とpending slotが一致する場合だけactiveへatomic promotionする。slot B実機bootでready前は`active=a,pending=b,attempted=b`を保持し、ready後だけ`active=b`へ昇格してpending/attemptedを消去。B active再起動でも保持を確認。Runtime promotionはtransactional updaterと同時に実装する。
- [x] journaled Runtime updaterと1世代rollbackを実装する
- [x] inactive-slot System updaterとreadback検証を実装する
- [x] Ed25519署名package builder/verifierと公開鍵を実装する
- [x] FE System Update画面とsafe reboot flowを統合する
  - 2026-08-13: `tests/test-pixel2-update.sh`とARM64 System chroot検証は合格。公開鍵だけをSystemへ同梱し秘密鍵混入gateを追加。ADBからrequestしたsigned Runtime/Systemの実機成功経路は合格。FEからのrequest、進捗/失敗表示、実機failure rollbackはacceptanceとして継続する。
  - 2026-08-14: updater側のframebuffer書込みは存在したが、Pixel2 Systemがprogress rawを未収録だった。`89fa6a4`でverify/runtime/system/finalize/rollback/errorの6画面を生成・収録し、欠落をSystem verifierで失敗させる。修正版SystemでのLCD目視は継続。
  - 2026-08-15: wildcard-source旧packageのmtime自動選択を禁止し、明示update chainだけを自動対象にした。署名metadata先読みで約900 MiBの旧payload全走査も廃止し、実機no-candidateは30秒超から3秒へ短縮。System `6a9fdfe`をA/B適用しslot Aでhealth昇格。
- [ ] compact seed imageとfirst-boot後partitionをhost/実機検証する
  - 2026-08-14: 現行64 GB実機SDで最終境界（p2=8192 MiB、p3=残り49.7 GiB）、既存user data復元、cold boot mountには合格。この時点ではrelease seed側が未実装だったため項目を継続した。
  - 2026-08-14: release seed provisionerを実装し、16 GB sparse-cardで最終MBR、ext4 8 GiB、残容量FAT32、directory seed、中断resume、idempotency、既存p3保護までhost合格。新規compact imageからの物理Pixel2初回bootのみ継続。
  - 2026-08-14: `e1b05ed` compact imageを64 GB実機SDで初回bootし、約1秒でp2=8 GiB、p3=49.7 GiB、complete/userdata markerまで完了。旧p3 FAT32 signatureをpreserveしてformatを省略したため短時間だった。初回sessionだけprovisioner mountへinitがfallback tmpfsを重ねる不具合を検出し、通常再起動後はp3 49.7 GiB、required directory 10/10、FE/ADB、mount count 1に合格。`cf8e96f`で既存mount採用guardを追加し、修正版imageの初回session確認を継続。

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
- [x] FE全機能を共有plumOS contractと照合し、欠落をrelease blockerにする
  - 2026-08-14: MFと同じ89 setting ID、START 7項目、Apps 12定義/7 visible、NW Service 5項目を機械監査。欠落AppsをP1扱いしていた監査と、Pixel2だけserviceを隠す分岐を廃止した。詳細は`docs/developer/frontend-feature-audit.md`。
  - 2026-08-15: [実機functional audit](docs/validation/2026-08-15-pixel2-frontend-functional-audit.md)で主要backendを再検証。Runtime `9da9bc7`、System `6a9fdfe`、FE/ADB、Runtime/System health、Network service stop後のcleanupに合格。物理menu acceptanceは継続。
- [x] Pixel2 framebufferとgpio-key inputを自動選択してboot時にfrontendを起動する
- [x] frontendとADBの診断logをSTATE partitionへ保存する
- [x] 実機LCDでfrontend描画と90度回転を確認する
- [x] 実機でfrontendのbutton mappingを確認する
- [x] plumOS共通型のグローバル音量キー・SELECT+音量輝度サービスを実装し、自動起動と実ゲーム音量変化を確認する
  - 2026-08-12: `380a006` app-layer/SYSTEMでdaemon起動、`pixel2_joypad`/`gpio-keys` open、helper往復確認済み。SELECT+音量による画面輝度変更は実機確認済み。音量の実音声確認はゲーム起動後に実施する。
  - 2026-08-12: `c8150cc` app-layerでPixel2 audio-routerがRK817内部ルートにもruntime software gainを適用するようにした。NES起動中に音量ボタンで実音量が変化することを確認済み。

## Connectivity

- [x] USB FunctionFS/configfs ADBをbring-up時の既定保守経路にする
- [x] 設定未作成時はUSB ADBを保守経路として起動し、FEの明示OFF/ONとrecovery markerを提供する
- [x] UDC `not attached`時のbounded ADB再列挙を実装する
  - 2026-08-15: adbd生存+gadget bindだけを正常扱いし、`FUNCTIONFS_BIND` timeoutから復旧しない欠陥を`0b9b609`で修正。起動を待たせない4秒後check、異常時だけのrebind/単発restartをSystemへ統合。署名System `0.1.0-dev-0b9b609`のslot B昇格後、更新bootと通常rebootの両方でFE/ADB、UDC configured、healthy no-opを確認。[検証記録](docs/validation/2026-08-15-pixel2-adb-enumeration-recovery.md)
- [x] USB再接続後のstale FunctionFS transportを再列挙する
  - 2026-08-16: UDCが`configured`へ戻ってもhost transportがofflineのままになる
    実機ログを根拠に`45b4505`で専用`replug`とaction lockを実装。物理抜き差し後に
    ADB自動復帰、単一adbd、競合エラーなしを実機合格。
- [x] USB Wi-Fi dongle検出とwpa_supplicant経路を実装する
- [x] ADB列挙とshellを実機検証する
- [x] USB Wi-Fi上でSSH/SFTP/FTP/Sambaの認証・往復転送と再起動復元を実機検証する
  - 2026-08-17: `fd0fb34` RuntimeでADB/SSH/SFTP/FTP/Samba backendを実機確認。
    password認証、upload/download/delete、同一SHA-256、更新再起動後の5項目ON保持、
    component checksumは合格。さらに`192.168.10.147`のUSB Wi-Fi LAN越しで
    SSHログインとSFTP/FTP/Sambaのupload/download/deleteを反復し、全downloadの
    SHA-256一致と検証ファイル削除を確認した。

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
