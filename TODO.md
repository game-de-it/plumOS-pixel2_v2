# TODO

## Adopted architecture

- [x] 既存plumOS機のupdate/storage/frontend/emulator設計を調査する
- [x] Pixel2のRockchip prefix + System A/B + ext4 Runtime + FAT32 User構成を決定する
- [x] ownership、update、rollback、first-boot provisioning contractを文書化する
- [x] Pixel2の単一USB portをWi-Fi優先のdual-role OTGとし、ADBをSystem/Runtime/FE/buildから廃止する
  - 2026-08-20: adbd/FunctionFS/device-role helper、ADB監視・復旧、FE項目、USB Mode、
    host helper、build依存を撤去。旧`usb_mode`/`adb_enabled`/FAT markerは起動時に除去し、
    Wi-Fi資格情報、network service設定、ROM、BIOS、saveは保持する。
  - 2026-08-21: 保存済みSSIDだけを根拠に`otg_mode=host`へ固定すると、dongleを充電器へ
    差し替えても`usb/online=0`のままsinkへ戻れない循環を修正。初案のextcon gateは、
    実機でRTL8821CU接続中も`USB-HOST=0`だったため撤回した。stock OTG中のPHY BVALIDで
    chargerを優先し、bounded host probeが空なら必ずOTGへ解放する。FE Wi-Fi操作からの
    明示probeも追加。起動中充電とWi-Fi再挿入の実機acceptanceは適用後に行う。
  - 2026-08-22: `97bf49c`適用後のoffline logで、c820列挙直後にboot workerがDWC2を
    unbindし、意図的removeをRuntimeが物理抜去と誤認してOTGへ解放、並行probeも競合する
    regressionを特定。host切替後の自然列挙待ち、全role操作の単一lock、意図的遷移markerを
    実装しfixtureへ追加した。System/Runtime `4993d8c`でcold boot Wi-Fi、dongle抜去後の
    起動中充電（LED、FE表示、`battery/status=Charging`、最大約+1.248 A）、dongle再挿入後の
    `0bda:c820`/IPv4/SSH復帰まで実機合格。stock driverは充電中も`usb/online=0`のため、
    充電判定はbattery status/currentを使う。充電器を挿したcold bootだけ継続確認する。
  - 2026-08-22: 充電器を挿したままShutdownすると、RK817 `DEV_OFF`後はLED/充電animationが
    消え、cable再挿入でだけ復帰する未検証境界を発見。最初の直接
    `BOOT_CHARGING=0x5242c30b`書込み+sysrq方式は実機で通常OSが起動して不合格となり、
    kernel reboot notifierが後段で`mode-normal`を上書きする経路と整合した。充電中Shutdownは
    Linux `RESTART2("charge")`からstock `CONFIG_SYSCON_REBOOT_MODE`へ委譲し、非充電時だけ
    `DEV_OFF`を使うよう修正。Charging、満充電+BVALID、満充電+cableなしのfixtureと
    AArch64 frontend component build/checksumは合格。署名Runtime `16150d5`を実機へ
    通常transactionで反映し、FE ready、`runtime_healthy`、frontend/root checksum合格まで
    確認済み。Wi-Fi dongleを充電器へ差し替え、FEからShutdown後にcable再挿入なしで
    stock充電画面へ遷移することをoperator合格。続けて非充電時のShutdownも完全OFFを
    operator合格とし、充電有無の両terminal経路を完了。

## Implementation audit and release blockers

- [ ] v0.1.3更新後のPortMaster起動regressionを解消する
  - 2026-08-30: 公開Runtime updateのSHA-256とpayloadを再検査し、Rockbox専用SDL
    library、共通port launcher、frontend環境隔離はいずれもpackageへ収録済みであることを
    確認。正式v0.1.3実機でも同一hashをreadbackした。一方、v0.1.3で追加した同期
    pre-launch auditはRockboxだけで約6秒・203 ELFと共通library群を走査し、FE経路では
    その間black frameになる。中断時には追跡fileを消した後もpatcher/state bind mountが
    残り、次回以降の全portを`stale ... mount`で拒否する状態も再現した。auditを起動後の
    advisory処理へ移し、mount cleanupを再試行・既知stale mount回収付きにして、Rockboxと
    Blaze of Stormを含むPortMaster全体へ共通対策する。
  - 2026-08-30: `1d1195e`でadapter 50を実装。同期`--enforce`監査をport終了後の
    advisory監査へ移し、通常unmount再試行、既知mountの追跡外回収、最終lazy detachを
    共通launcherへ追加した。署名update `0.1.3-dev-1d1195e`を実機へ適用し、full Runtime
    verify、1秒以内のRockbox process開始、非black 640x480 DRM capture、終了後のFE復帰、
    mount残留0、追跡fileなしのstale-mount fixture回収に合格。GameMaker代表のTiny Rallyも
    同期監査に遮られず`gmloadernext.aarch64`開始、終了後回収に合格したが、検証機に
    Blaze of Storm本体がないため同titleの画面・操作は利用者再試験待ち。
  - 2026-08-30: v0.1.4実機でTiny Rallyと、過去に表示合格済みのApotrisがともに
    game process開始前に黒画面となる回帰を確認。FEの実体は`plumos-frontend-pixel2`
    だが、共通port launcherの祖先判定が旧内部名だけを認識していた。この誤判定で
    launcherが親FEへstopを送り、FEのforeground process-group回収がlauncher自身を
    `SIGTERM`で終了させ、FE停止とprivate bind mount残留を引き起こしていた。adapter 51は
    `/run/plumos/frontend.pid`の実PIDを祖先chainで照合し、現行binary名もfallback認識する。
    host runtime、mount cleanup、session cleanup試験は合格。実機FE経路でApotris/Tiny
    Rallyの表示、終了、FE復帰、mount 0を再確認する。

- [x] PyxelのPixel2実機acceptanceを完了する
  - 2026-08-20: 全Pyxelタイトルが`EGL not initialized`で終了する原因を、存在しない
    root EGL pathと誤った`panfrost` loader強制に特定。PortMaster実機合格経路と同じ
    `apps/pyxel` EGL/GLES、`rockchip_dri.so`、Mesa自動選択へ修正し、SSHの10秒bounded
    launchではwindow初期化失敗が消えた。続いて汎用`plumos_output`でのPyxel固定
    22.05kHz mono stream拒否を特定し、既存の専用`plumos_pyxel` PCMへ接続した6秒
    bounded launchも終了要求まで継続。
  - 2026-08-20: native 480x640 DRM scanoutへ270度presentするPyxel専用GL adapterを
    shared plumOS sourceからcomponent内に生成し、shader-fitを標準有効化。実機で
    Last Emulatorの720x480（3:2）が640x427、factor 0.888889、上下約27pxへ等比縮小
    され、全端が画面内に収まるcaptureを確認。SDL hardware cursorも非表示化した。
    初回正式反映の目視で、shader-fitより後段の固定640x480回転FBOが720x480 sourceを
    先に切り、log上は640x427でも実画面が左寄り・右見切れになることを検出。A30で
    Last Emulatorを合格させた方式に揃え、`pyxel.init()`の実canvas寸法をOS shimから
    presenterへ渡し、source全体を保持した最終回転段で任意解像度を等比fitする設計へ変更。
    checksummed Runtime反映後にFE起動、実LCDの向き、入力、終了hotkey、音声を
    operator確認する。Last Emulator固有のpygame/Pyxel二重audio初期化は継続課題。
    物理AがSDL既定のBとして解釈されタイトルから終了していた入力漏れには、PortMaster
    実機合格済みのPixel2 controller GUIDと`a:b1,b:b0,x:b2,y:b3`をPyxel launcherへ
    共通適用した。production-equivalent動的試験で物理AからStart Gameへ遷移し、ゲームを
    操作できることをoperator合格。正式Runtime `0.1.0-dev-f4aadf3`もchecksum整合反映済み。
    FE導線からの再起動、Bで戻る、終了hotkey、音声のoperator確認を継続する。
    76 MB級pyxappの短時間再試験で公式CLIの5分stale猶予中に`/tmp`が満杯になるため、
    live PIDの展開物を保持したままdead PID分を起動前に回収し、正常・異常終了時は自身の
    展開rootだけを削除するshim cleanupも追加した。
  - 2026-08-23: FE導線からの起動・復帰、物理操作、終了、表示、実音声をoperator合格とし、
    Pixel2実機acceptanceを完了した。
  - 2026-08-28: Rockbox終了後に復帰したFEがPortMasterの`LD_PRELOAD`を継承し、
    次に起動したPFSへRockbox/PortMaster/Pyxelの描画shimが同時注入されるため、音声だけ
    再生してDRM面が黒1色になる順序依存不具合を特定。`plumos-frontend-launch`はFEを
    `env -i`の最小環境から起動するよう変更し、host回帰試験、strict app-layer、署名
    Runtime `0.1.2-dev-508b567`、実機full runtime verifierに合格した。修正後PFSは
    Pyxel専用shim 2件だけを読み込み、非黒画面、ALSA pointer進行、operatorの表示確認に
    合格。PortMasterや他appから復帰したFEを次のゲームの環境正本にしない契約を追加した。

- [x] SDL/KMSのマウスカーソルがFE/PortMasterへ残留しないようにする
  - 2026-08-20: 実機DRM captureで64x64 ARGBのhardware cursor plane 69を確認。
    PortMaster adapter 39はcontroller-only UIでSDL cursorを常時非表示にし、FEは
    primary CRTC取得後に前clientが残したtype=cursor planeだけを無効化する。
    overlay/primary planeには触れず、他アプリからFEへ戻る経路も共通で復旧する。

- [x] FE起動前にPixel2の基底framebufferを毎回クリアする
  - 2026-08-20: 初回setup/update/recoveryの最終画面がDRM clientの背後に残り、
    game/app切替時に一瞬再表示される問題へ対処。Systemがnative
    480x640 XRGB8888のopaque-black frameを生成・検証し、`40-frontend`の共通起動経路で
    FE process生成直前に`/dev/fb0`へ書き込む。ゲーム起動前の既存FE側クリアも維持する。

- [x] Wi-Fi dongleを挿したままのcold bootと物理再挿入を自動復旧する
  - 2026-08-20: stock DTBの`dr_mode=otg`は保持し、Wi-Fi設定済みかつUSB upstream
    非接続時だけRockchip stock sysfs ABIの`otg_mode=host`をDWC2 bind前に適用する。
    boot時の挿しっぱなしdongle列挙漏れを防ぎ、起動後の再挿入はV90S準拠のblocking
    uevent monitorからwpa/DHCP/network serviceを自動再開する。常時pollingは追加しない。
  - 2026-08-21: cold-boot force-host条件へextcon `USB-HOST=1`を追加した初案は実機反証に
    より撤回し、OTG中のPHY BVALIDと実downstream列挙を使うbounded probeへ置換。charger、
    dongle抜去、空probeではPHYをstock `otg`へ戻す。RTL8821CUのstorage-mode ejectだけは
    5秒遅延後にdownstream不在を再確認し、意図した`1a2b -> c811`再列挙を壊さない。

- [x] System A/B更新時にslot metadataを全て同一transactionで更新する
  - 2026-08-20: SquashFS、`.sha256`、署名`manifest.json`/`.sig`に加え、image生成時の
    text `system-{a,b}.manifest`もinactive slotへのreadback後、pending commit前に原子的に
    更新する。package builderはSystem内の`source_ref`と`source_date_epoch`を署名対象へ
    取り込み、次回以降のcaptureでfactory seed情報が残らないようにする。

- [x] build target、FE導線、runtime helper、Apps、standalone、storage/update、release準備を横断監査する
  - 2026-08-13: [実装リスト](docs/developer/implementation-status.md)を更新。SaturnをRK3326性能要件で非対応化し、97 system中87 enabled、109 libretro core、standalone 4 built / 4 pendingを現行baselineとする。
- [x] FE catalogと生成app-layerの不整合を検出する自動監査を追加する
  - `./scripts/docker-build.sh audit`は開発中のreport、`audit --release-gate`は公開済み未実装があれば失敗する。`release-image`へrelease gateを統合。
- [x] P0 user surface blockerを0件にする
  - [x] one-shot boot後もPID 1にchild reaperを保持する
    - 2026-08-22: 現行Systemは最初のFE終了後にPID 1をlogin shellへ置換し、42 routeの
      machine smoke後にPPID 1のzombieが186件蓄積した。boot setup後は明示inittab付き
      BusyBox initをexecするよう修正し、隔離PID namespaceで孤児childの回収、System
      A/B rootfs build/verifyに合格。smoke validatorもprofile別の正規stop helperを
      優先し、全process-group同時killを廃止した。
  - [x] 修正版Systemを署名更新し、実機でPID 1=`busybox init`、FE→game→FE後のzombie増分0を確認する
    - 2026-08-22: `4993d8c -> 51e5ebd`のexact-source署名Systemを通常A/B更新。
      inactive Bのreadback SHA、B boot/active、`system_healthy`、PID 1 BusyBox initに合格。
      NES、Channel F、ColecoVision起動後もzombieは0のまま。
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
    - 2026-08-23: 物理menu選択だけをtest helperへ置換し、実overlay ownership、
      RTC wake、DRM capture、ALSA pointerをRA/SA/PicoArch/Appsで共通検査する
      `validate-pixel2-sleep-matrix.py`を追加。FDS/FCEUmm、PPSSPP、FBNeoはkernel
      `mem` suspend、同一process group、前後画面、復帰後音声を機械合格。
      Music Playerはblocking ALSA writeがresume後`SUSPENDED`で止まる問題を検出し、
      nonblocking writeと`EAGAIN/EPIPE/ESTRPIPE`回復を実装。`f57f2b7`ではkernel
      resume時のUSB removeを物理抜去と誤認しないmarkerと、既存host probe/Wi-Fi
      recoveryの非同期再開も追加した。署名Runtime `0.1.0-dev-f57f2b7`でRA、PPSSPP、
      PicoArch、Music Playerのpause、kernel sleep、同一process、前後画面・音声、
      Wi-Fi/SSH自動復帰を全て機械合格。物理Power操作と目視・可聴品質だけをoperator
      acceptanceとして残す。
    - 2026-08-24: Sleep充電の2経路を実機合格。充電器接続後のSleepは画面を完全消灯した
      まま15%から72%まで充電し、未接続Sleep後の充電器挿入もkernel wake後に表示復帰を
      保留してsoftware standbyへ戻る`e256391`で、消灯維持・充電・物理Power復帰に合格した。
      復帰直後の単発Wi-Fi probeが充電器を見て終了し、後から挿したdongleを拾わない問題は、
      Sleep復帰後だけ120秒・3秒間隔で既存host probe/recoveryを再試行する`b66efd3`で修正。
      10回目に`0bda:c811`を再列挙し、再起動なしでIPv4 `192.168.10.107`へ復帰した。
      frontend 199件、app-layer 11,267件、Runtime verifierも合格。
      [検証記録](docs/validation/2026-08-24-pixel2-sleep-charging.md)を参照。
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
    - 2026-08-22: Wi-Fi実機でSSH login、SFTP/FTP/Samba `SDCARD`の1 MiB
      upload/download/deleteを再検査し、全SHA-256一致。macOS `smbutil view`のshare
      列挙だけ`Broken pipe`だが、`mount_smbfs`による直接mountと実転送は合格。
  - [x] ADBのboot既定値・UI設定・recoveryを一貫させる（2026-08-20 方針変更により廃止）
    - 2026-08-19: Pixel2の単一OTG portに独立したWi-Fi/ADB checkboxを持たせる設計を
      廃止。FEを`USB Mode = ADB / Wi-Fi / Off`の単一選択へ変更し、永続`usb_mode`を
      System adbd、USB host再列挙、Wi-Fi boot、Runtime scan/connect/recoveryの共通
      ownership正本にした。旧設定はmigration fallback、FAT markerは緊急ADB override
      として保持。3 modeのhost fixture、FE cross-build、6言語key整合は合格。署名
      System/Runtime `0.1.0-dev-21fba08`を署名package化し、実updaterの署名・source
      version・ABI・manifest検証に合格。System Aを完全backup後offline置換し、署名・
      readback、inactive B、ADB marker保持を確認。Runtime deltaもFAT32 inboxへ配置済み。
      cold boot後にFEからRuntime updateを要求し、ADB、抜き差し、Wi-Fi切替を実機確認する。
    - 2026-08-19: offline read-only captureでSystem Aは`21fba08`だが、実Runtimeは
      `8a98e3e`のままと確定。前回deltaは誤って`5535fa8`をsourceにしていたため適用対象外で、
      排他USBをまだ実機試験できていなかった。採取した4,261-file checksumをbaseに
      `8a98e3e -> 21fba08`の24-file署名deltaを再生成し、実updaterの署名、exact source、
      ABI、manifest、payload検証に合格。FAT32 inboxの当該packageとsidecarだけを置換し、
      SHA-256 `ac393c80...`のreadback一致を確認。FE System Updateから適用後、healthy昇格、
      `usb_mode=adb` migration、cold boot、物理抜き差し、ADB実transportを継続する。
    - 2026-08-19: 正しいRuntime適用後もmacOSでは`18d1:4ee7`とADB interface
      `255/66/1`、bulk endpoint 2本まで列挙された一方、ADB 35/36ともtransportを
      作れず、host logはbulk-IN `0x81`のclear-haltを`LIBUSB_ERROR_OTHER`で拒否した。
      family実績と比較し、Pixel2だけadbdがnonblocking FunctionFS AIOのままだった実装漏れを
      特定。`5246728`でPixel2 stock kernel向けlegacy FunctionFS＋同期I/Oへ統一し、ARM64
      build、recovery fixture、System rootfs gateに合格。`21fba08 -> 5246728`の署名System
      package（SHA-256 `164f9c6e...`）も実updater inspect済み。offline deploy後のA/B
      promotion、実shell、cold boot、抜き差しを継続する。
      - 62.5 GB実カードのSystem A=`21fba08`とinactive Bをread-only確認後、署名Systemと
        sidecarだけをFAT32 update inboxへ追加。card readback、実updater inspect、両slot
        SHA不変、zero-byte ADB marker保持を確認した。active slotを直接上書きせず、FEから
        通常A/B更新を要求してinactive書込み・promotion・rollback契約を維持する。
      - その後のread-only captureでFEがSystem requestを生成しておらず、bootも
        `no-pending-state`でSystem A=`21fba08`のままと確定。同期FunctionFSをまだ実機試験
        できていなかった。ADB不能中の確実な復旧として、旧System Aの5管理ファイルを
        PLUMOS_USERへ退避した後、署名済み`5246728`をSystem Aへoffline適用。署名、
        manifest/payload、SquashFS readback SHA-256 `99ce6ed9...`は一致し、inactive Bは
        `2a6170fe...`のまま保持。cold boot shellと物理抜き差しを継続する。
      - `5246728`のcold bootもFE `waiting`で不合格。最初の`10fc87a`自体が同期
        FunctionFSで、最初の安定化`6f022d4`でnonblockingへ変更された履歴を確認。
        cold boot/shell/物理抜き差しの実機証跡が揃う`45b4505`をrollback基準とし、
        nonblocking adbd、単発4秒health check、物理online後の単発2秒replug、PID lockを
        復元。protocol-state patchとkernel uevent monitorは撤去し、後続機能と現行
        `usb_mode`契約は保持。ARM64 adbd、strict app-layer、System A/B rootfsのbuildと
        host gateは合格。実機の`System 5246728 / Runtime 21fba08`から`11a7f94`への
        署名packageを生成し、実updaterのexact source・ABI・署名・payload検証に合格。
        62.5 GB実カードのSystem A=`5246728`を5ファイル退避後、System Aを
        `11a7f94`へoffline適用。署名、manifest/payload、readback SHA-256は一致し、
        inactive BとADB markerは保持。Runtime packageはinboxへreadback済みだが未適用とし、
        まずSystem adbdだけのcold bootを分離検証する。
      - System-only `11a7f94` cold bootもFE `waiting`。macOSは`18d1:4ee7`親deviceだけでなく
        ADB interface `255/66/1`、bulk IN/OUT、512-byte endpoint、configuration 1まで列挙。
        host ADB 36 native backendを再起動しても、最初のCNXNでread/writeとも即座に
        `e00002ed` (`kIOReturnNotResponding`)。FE表示やinterface欠落ではなく、端末側
        bulk endpointが列挙後に応答不能なので、同bootの永続adbd/init logを回収する。
      - Runtime partitionのread-only採取で、`11a7f94`の同bootは`22:47:02`
        `FUNCTIONFS_BIND`後、hostがconfigurationを選ぶ前の`22:47:06`に4秒startup
        watchdogがUDC `not attached`を異常扱いし、正常な初回adbdを停止していたと確定。
        再起動instanceは`FUNCTIONFS_BIND`後に`FUNCTIONFS_ENABLE`へ到達しておらず、hostの
        `kIOReturnNotResponding`と一致する。`c41698a`でnonblocking FunctionFS、明示的な
        物理`replug`、PID lock、排他`usb_mode`は保持し、自律startup timerだけを完全撤去。
        slow-host fixtureは5秒間`not attached`でも同一adbd PIDを保持し、watchdog不在を
        検証する。修正を含むSystem `1eab72a`を署名package化し、実カードのinstalled
        `11a7f94`に対するexact source・ABI・署名・payload検証に合格。System Aを5ファイル
        退避後offline置換し、署名とSquashFS SHA-256 `64bfc14e...`のreadback、inactive B、
        ADB marker保持を確認。cold boot shellと物理抜き差しを継続する。
      - `1eab72a`適用後のread-only captureでstartup watchdog撤去は有効と確認。
        `23:11:02 FUNCTIONFS_BIND`後に同一adbdは維持されたが`FUNCTIONFS_ENABLE`へ
        到達せず、macOSのconfiguration 1選択後も最初のCNXNが
        `kIOReturnNotResponding`となった。またFEのWi-Fi/Off/ADB操作は全て
        `apply=reboot-required`として設定保存だけを行い、実際のSystem ADB停止・
        再生成を一度も実行していなかった。`556704d`で明示切替を即時排他動作へ
        変更し、ADB選択時はUDC unbind、adbd停止、FunctionFS unmount、configfs
        gadget削除から完全再生成する。通常bootのstart-enabledはhealthy gadgetを
        維持し、Wi-Fi/OffはADBを即時停止する。ADB recovery、USB mode、Wi-Fi、
        USB host、network control、strict app-layer、power/sleep、System rootfsの
        host gateは合格。最終ref `644c77d`のFrontend/Network/Systemを並列buildし、
        strict Runtimeを生成。実カードの`System 1eab72a / Runtime 21fba08`をexact
        sourceとする署名packageは署名・ABI・manifest・全payload検証に合格し、FAT32
        update inboxへreadback一致で配置済み。Systemを最新mtimeとして通常A/B更新を
        先行させ、その後Runtimeを適用してOff -> ADB実機確認を継続する。
      - `644c77d`適用後もFE `waiting`で不合格。複雑化した排他制御、自動recovery、
        UDC状態判定を追加する方針を中止し、ADBが通常接続できていた初期安定化
        `6f022d4`の`10-adbd`とdaemon-only watchdogをbyte-for-byteで復元した。
        `10-adbd`はFE設定、Wi-Fi、`usb_mode`、marker、replug/recover/statusを一切参照せず、
        FunctionFS作成、nonblocking adbd起動、ep1/ep2待機、UDC bindだけを行う。
        現Runtimeや保存設定によるUSB host化を排除するため、復旧Systemではimmutable
        `/etc/plumos-adb-only`によりUSB host再列挙、USB Wi-Fi、Runtime network service
        bootstrapを休止する。`f30be85`と`fde06bb`に分割コミットし、exact SHA gate、
        System script、dispatcher、ARM64 rootfs検証は合格。System
        `0.1.0-dev-fde06bb`を生成済み。read-only captureでactive/booted A、pendingなし、
        System `1eab72a`、Runtime `21fba08`、`usb_mode=adb`を確定。署名packageをexact
        sourceとして実updater検証後、旧System Aの5管理ファイルをPLUMOS_USERとhostへ
        二重退避し、Aだけをoffline置換した。署名とreadback SHA-256
        `7d71d2f6...`は一致し、inactive B、Runtime、zero-byte ADB marker、全user dataは
        不変。cold boot shellを最優先で確認する。Wi-Fi/USB mode再導入はADB合格後の
        別作業とする。
      - `fde06bb`を通常A/B updaterでactive/booted Aへ昇格して初めてbaselineを実機試験。
        macOSは`18d1:4ee7`親device/configuration 1まで列挙したがADB interfaceはなく、
        `adb devices`も空で不合格。さらにimmutable `/etc/plumos-adb-only`がSystemを常時
        ADB起動に固定する一方、Runtime `21fba08`は保存済み`usb_mode=wifi`からWi-Fiを
        起動し、同一DWC2上でadbdのep0/1/2 FDとRTL8821CU `0bda:c820`が同時に存在する
        ownership競合をSSHで確定した。Systemはboot前にFAT recovery markerと
        `usb_mode`からownerを一度だけ決定し、wifi/offではadbdとwatchdogを起動しない。
        temporary adb-only markerを撤去し、Runtimeは現行の即時排他切替へ更新する。
        旧`6f022d4` ADB lifecycleは維持し、FE API互換のread-only `status`だけを追加。
        host fixture一式は合格、署名System/Runtimeの実機適用とOff/Wi-Fi/ADB/cable replug
        acceptanceを継続する。
      - 上記System初回packageは署名targetを`4ddb809`とした一方、hostに`unsquashfs`が
        無いためbuilderがembedded checkを黙ってskipし、実System versionが
        `0.1.0-dev`のままになっていた。ownershipとB healthyは合格したがRuntime適用を
        中止。System package生成は`unsquashfs`不在をerrorにし、tools container内で
        embedded version/ABI一致を必須化した。version明示Systemをlive embedded
        `0.1.0-dev`からのcorrective updateとして再適用する。
    - 2026-08-14: Wi-Fi非搭載Pixel2で保守経路を失わないよう、設定未作成時だけADBを既定ONへ戻した。FEで保存した`adb_enabled=0/1`を最優先し、FAT32 rootの`plumos-enable-adb`は明示OFFからも復旧できる。新Systemの実機cold boot確認は継続。
    - 2026-08-14: ADB不能SDへ`67c25aa` System dispatcher/A/Bをoffline recoveryとして適用。旧`d56bf29`一式はFAT32 user volumeへ退避し、stock Image/DTB、Runtime、ROM、BIOS、設定を保持。cold boot ADB確認とRuntime transactional updateは継続。
    - 2026-08-16: USB給電のoffline/online transitionを常駐hardware-key serviceで
      検出する。`45b4505`では通常`recover`と物理再接続`replug`を分離し、再接続
      2秒後だけUDC rebind、失敗時は単発clean restartを行う。全mutating actionを
      PID lockで直列化し、二重adbd/JDWP/FunctionFS競合を防止した。署名System/
      Runtime `0.1.0-dev-45b4505`を実機適用し、USB抜き差し後のFunctionFS
      BIND/ENABLE、adbd 1 process、UDC configured、ADB shell復帰を確認。
    - 2026-08-18: VBUSを維持したmacOS側の論理USB再設定ではFunctionFS transport
      だけが破棄され、UDC configuredとadbd生存をhealthyと誤判定する欠陥を確定。
      `8a98e3e`でowned adbdがprotocol online/offline markerを出し、offlineが3秒
      継続した場合だけhardware-key daemonが単発replugする方式へ変更した。自然復帰時の
      無条件二重rebindは廃止。署名System/Runtime `0.1.0-dev-8a98e3e`を実機適用し、
      host server自然復帰でPID維持、VBUSありUDC unbindから8秒・ケーブル操作なしで
      ADB復帰、各health、再起動後保持を確認。物理抜き差しの最終確認を継続する。
    - 2026-08-18: V90S準拠Systemの初回実機試験はFE `RUNNING`まで到達したが、host
      transportは`offline`、物理抜き差し後は`waiting`となった。さらにNW Serviceの
      ADB OFFがFAT ownership markerを削除する一方、ONが再作成しないため、保存Wi-Fi
      設定が唯一のOTG portをhost roleへ戻す欠陥を特定。ADB ONでmarkerを作成し、
      System起動watchdogもUDC `configured`かつtransport `offline`を単発replugする。
      `0694c47`の署名Systemをactive Aへoffline適用し、署名・SHA-256 readback、zero-byte
      marker、inactive B保持を確認。Runtime deltaも更新inboxへ配置済み。cold boot後に
      Runtimeを適用し、抜き差し、OFF→ON→再起動を再確認する。
    - 2026-08-18: `8a98e3e`以降の物理抜き差しでADBがwaitingとなり、再起動・cold
      bootでも復帰しない回帰を確認。`usb/online=0`を根拠にhost roleへ変更すると、
      Mac接続後も同じ値が0のままになりdevice roleへ戻れない循環だった。`ecf4d16`の
      短時間probeも実機不合格。V90S `d1721a9`の「ADB有効中はbound gadgetを保持し、
      disconnect後は同じgadgetをrebind」するcontractへ戻し、Pixel2固有差分を保存済み
      Wi-Fi時の明示host assignmentだけに縮小した。新System/Runtimeのoffline適用、
      cold boot、物理抜き差し、Wi-Fi saved-ON bootの再acceptanceを継続する。
    - 2026-08-19: repository開始からADB履歴を再監査。stockOS `SYSTEM`本体は方針通り
      保存されていないためstockOS userlandのADB有無は未証明。一方、`d18bc0e`のexact
      stock Image/DTB移行後、`aff4656`の保存logでstock kernel 5.10.198、空のusb-role、
      FunctionFS BIND/ENABLE、UDC configured、実ADB shellを確認。`5932ef9`、`e47ce97`、
      `45b4505`にも独立したshell/update/replug成功証拠がある。回帰境界はcharger値で
      ADB開始を抑止した`2e2077f`、DWC2 host rebindの`49a4f15`、VBUS DTB変更の`b1f6228`
      以降。`8f3e5f5`のMMIO device-forceも実機waitingで不合格。次は失敗log回収後、
      `aff4656`時点の単純な非同期FunctionFS lifecycleへ戻し、現行からはADB priority
      guardだけを残してcold boot shellを先に再合格させる。
    - 2026-08-18: `0694c47`のcold bootもFE `waiting`で不合格。macOSには
      `18d1:4ee7 plumOS Pixel2 ADB`親deviceだけが列挙され、FunctionFS interfaceと
      `adb devices` entryが無かった。旧Runtimeのprotocol-offline 3秒replugと新Systemの
      4秒startup replugが連続した競合と確定した。protocol stateを診断専用へ戻し、
      hardware-key pollingを削除、V90S `d1721a9`と同じblocking kernel uevent monitorで
      `android_usb/DISCONNECTED`時だけ単発replugする。新System/Runtimeのoffline適用、
      cold boot、抜き差し、OFF→ON→再起動を再確認する。
      - `5535fa8`の完全System/strict Runtimeを署名package化し、signature、manifest、
        全payload SHA-256/size、System squashfs、app-layer checksumをhost検証済み。
        active A `0694c47`を完全backup後、`5535fa8`へoffline置換し署名・readbackを
        検証した。inactive B、ROM、BIOS、設定、saveは保持し、Runtime packageとADB
        markerもFAT32へ配置済み。4項目の実機acceptanceを継続する。
    - 2026-08-19: `5535fa8`もcold bootで`waiting`となったため、ext4 partitionを
      read-only mountして永続logを採取。`FUNCTIONFS_BIND`後、hostの`ENABLE`前に残存
      4秒startup watchdogがUDC `not attached`を異常扱いし、rebind失敗からadbdを
      再起動していたことを時刻付きで確定した。V90Sにはこの起動timerはないため完全
      撤去し、kernel disconnect helperも強制`replug`ではなくV90S同様`recover`だけを
      呼ぶ。`f8a5608`の署名System packageはsignature、payload SHA-256/size、slow-host
      5秒保持を含むhost gateに合格。active A `5535fa8`を完全backup後、`f8a5608`へ
      offline置換し署名・readbackを検証した。inactive Bと全user dataは保持。cold bootを
      継続する。
    - 2026-08-19: `f8a5608`でもmacOSに親deviceだけが列挙され、FE `RUNNING`が実transport
      を保証しないことを再確認。保存済みWi-Fi導入後も`15-usb-host-reenumerate`と
      `20-usb-wifi`がADBとは独立に同じDWC2を制御し、ADB bind後にhost reset可能な競合を
      特定した。ADB ONを唯一の最優先ownerとし、System host再列挙、Wi-Fi boot、Runtime
      hotplug/scan/connectを全て抑止する。ADB OFF時だけWi-Fiを許可するhost fixtureと全
      app-layer gateは合格。`1e065fb`の署名System/strict Runtimeをpackage化し、実updater
      で署名・source version・ABI・target・manifestを検証済み。offline適用、cold boot、
      物理抜き差しを継続する。active A `f8a5608`を完全backup後、System `1e065fb`へ
      offline置換し、署名・SHA-256 readback、inactive B保持、zero-byte ADB markerを確認。
      Runtime deltaもFAT32 update inboxへreadback一致で追加済み。cold boot後の実transport、
      Runtime適用結果、物理抜き差しを継続する。
    - 2026-08-19: ADB 35/36、両macOS backend、新USB serialの全てで最初の`CNXN`が
      `kIOReturnNotResponding`となり、Mac再起動後は給電中Pixel2が親deviceだけを再列挙して
      ADB interfaceを失うことを確認。host cacheではなく、host disconnect後のPixel2側
      FunctionFS復旧欠落と確定した。さらにADB監視PID fileが親shellを指し、Wi-Fi切替後も
      実monitorが残る欠陥を`662cace`で修正。`d7142ac`ではV90S `d1721a9`/`e2fe3c3`と同じ
      blocking kernel uevent方式へ変更し、`android_usb/DISCONNECTED`時だけ同じadbdと
      endpointを保った単発UDC rebindを行う。自己生成eventは4秒だけ抑止し、timer、polling、
      recurring restartは持たない。PID lifecycle、ADB/Wi-Fi排他、event filterのhost gateは
      合格。全コミットを含む`636b64c`の完全Systemとstrict Runtimeを現行実機
      `7c72d9a`からのexact-source署名packageとして生成。実updaterの署名、target、ABI、
      source、manifest、全payload検証に合格し、Runtimeはroot/component metadataを含む
      12管理entry・削除なし。適用後のcold boot shell、抜き差し、Mac再起動、Wi-Fi往復を
      実機acceptanceとして継続する。
    - 2026-08-19: 実機で一時的に`device`へ到達した直後、kernel disconnect監視が
      UDC/FunctionFSを再設定してtransportを自ら切断し、`waiting`または`STOP`へ戻す
      feedback loopを確認。最初のADB実装`10fc87a`には自動監視がなく、初期安定化
      `6f022d4`にもdaemon消滅だけを見るwatchdogしか無かったため、後から追加した
      USB state monitorを回帰要因と判断した。`dacbc83`でkernel uevent listener、
      recurring watchdog、cable/host event起点のrebindを全撤去し、ADB構成変更を
      USB Modeの明示的なADB選択とsleep resumeだけに限定した。System/Runtime
      `0.1.0-dev-dacbc83`をexact source (`626a1e8` / `f5ca09e`)から署名更新し、実機で
      両version、System script SHA-256、監視file/process不在、Runtime全checksumを確認。
      Off -> ADB、物理ケーブル接続、抜き差し後の実transportを最終acceptanceとして継続する。
  - [x] `plumos-thumbnail-scraper`、scraper sources、plan/fetch/result導線を実装する
    - 2026-08-13: MFの実績あるrunnerをPixel2のmulti-line catalogと`/mnt/plumos-user`へ適合。owned curl/dependency/CA bundle、AppsのScraping導線、atomic PNG、cache、bounded fetchを統合し、ROM setのNES 1本でCRC match/download成功をhost検証。
    - 2026-08-21: 実機の初回NES scrapingで2つのCRC workerが同一DAT・thumbnail一覧を競合取得し、118件すべてが`skipped_tool`になる回帰を確認。`7ee72d7`で共有index生成を排他化し、`c1cd822`でworker開始前の親process preflightへ変更した。空cache・2 workerの実機試験は2/2 download、skip 0。全118 ROMでは既存2件に105件を追加し、CRC miss 5、thumbnail miss 6、download failure 0、最終107画像。FE library scanも118 ROM中107 thumbnailを解決した。
  - [x] `plumos-sdcard-cleanup`をPixel2 storage contractへ実装する
    - 2026-08-13: `/mnt/plumos-user/{roms,images}`を既定scopeとし、macOS/Windows sidecarだけを削除するbounded lock/interval/dry-run/cache-invalidation helperを追加。host fixtureでdry-runと削除を検証。
  - [x] ch/pt/fr/de translationを同梱し、6言語のkeyを検証する
    - 2026-08-13: en/jaを含む6言語すべてが364 keyで一致し、Pixel2以外のdevice/distro identityがないことをhost検証。実機glyph/折返しは未検証。
  - [x] arduboy、megaduck、puzzlescript、superbroswarのtheme logoを追加する
    - 2026-08-13: 190x156 RGBのPixel2 theme badgeを再現可能なgeneratorから生成し、frontend component/checksumへ統合。
  - [x] 公開中の`standalone:pcsx_rearmed`を実装・実機検証する
    - [x] PCSX-ReARMed r26l、sdl12-compat、Pixel2回転fbdev、入力、48 kHz音声、factory configを再現可能にhost buildする
    - [x] PCSX-ReARMedを実機で起動し、画面、全入力、音声、menu/exit、FE復帰を確認する
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
  - [x] Wi-Fi経由の公式catalog更新、Ready-to-Run portのinstall・起動・音声・入力handoff・表示・FE復帰を実機検証する
  - [x] PortMaster共通互換レイヤーで未知のAArch64 portを事前監査・安全実行する
    - [x] install済みlauncherとELFを静的監査し、不足SONAME、ARMHF、危険な
      `LD_LIBRARY_PATH` / `LD_PRELOAD`置換、未対応host commandを機械判定する
    - [x] port scriptが環境変数を上書きしても、子ELFの実行境界でPixel2必須の
      library path、SDL/OpenGL補正、session identityを再合成する
    - [x] 異常終了、background化、二重forkを含むprocess treeを回収し、gptokeyb、
      session mount、audio/DRM ownershipを解放してFEを一つだけ復帰する
    - [x] Moonlight Newをnetwork/video系representativeとしてhost gate・実機試験へ
      追加し、正立表示、監査0件、終了回収、FE復帰を確認する
    - [x] Rockboxを独自preload/scaler系representativeとして実機合格させる
      - adapter 49でGUI texture生成後にPixel2 panel向けresizeを適用し、正立表示、
        D-pad/決定/戻る、音楽再生、`START+R2`終了、FE復帰をoperator合格。Rockbox終了後の
        private表示環境がFEと次のPyxelへ漏れる問題も`508b567`で共通遮断した。
    - [x] 起動時間を増やさず、build/CI、PortMaster更新後、初回port起動時だけ監査し、
      package hashが同じportの結果を再利用する
    - 2026-08-27: adapter 48でpure-Python ELF audit、SONAME単位のAvahi/nghttp2投影、
      `execve`/`posix_spawn`境界の環境guard、session identityによる孤児process回収を
      共通化。runtime SONAME symlinkを実機監査対象に含め、license本文はcommand監査から
      除外。cleanup helperへ別session IDを与え、PPid鎖とKILL後の収束待ちで自己・
      終了途中PIDの誤検出を防止。Moonlight型と
      Rockbox型のhost fixtureは合格。adapter 48をstrict buildし、署名Runtime
      `0.1.2-dev-b0706c8`として実機deploy、完全checksum合格。Moonlightは合格。
      Rockboxはadapter 49と署名Runtime `0.1.2-dev-679e627`で画面・入力・音をoperator
      合格し、`508b567`で次appへの環境漏洩回帰も解消した。
  - [ ] Moonlight Newのペアリング後接続とPixel2可読性を実機再確認する
    - 2026-08-30: ペアリング自体と同梱key、host `192.168.10.100`への直接`moonlight list`は
      成功する一方、LÖVE GUIが非同期list完了前に`apps.txt`を読み、仮項目
      `Load apps first`をstream対象にして終了する競合を特定。adapter 52はlistだけを完了待ちし、
      Pairは従来通り非同期、仮項目は選択対象から除外する。Pixel2の20px相当fontには28pxの
      下限を設け、空文字列を実行していたPortMaster共通`ESUDO`には安全なexec shimを追加。
      既存GUIをhash付きでbackupしてから原子的にpatchし、更新済み内容への二重適用もしない。
      Lua構文、既知upstream fixture、PortMaster runtime、app-layer script回帰はhost合格。
      実機でReload Apps、host選択、stream開始、文字サイズ、終了後FE復帰を確認する。
    - 2026-08-30: adapter 52実機でGUI正立、28px下限、`Steam Big Picture`/`Desktop`一覧、
      Desktop stream開始、ALSA pointer進行まで合格。stream本体だけSDL GLES2 rendererで
      R/Bが逆転することをDRM captureで確定し、Moonlight限定でSDL software rendererへ
      切り替えた比較試験では同一Desktopが正しい色になったことをcaptureとoperatorの双方で
      確認した。他portの既定GLES2経路は変更しないadapter 53として正式化し、終了後FE復帰と
      managed checksumを再確認する。
  - 2026-08-14: 共有7 Appsをcatalog、component manifest/checksum、visible launcher存在gateへ統合。host build済み。各Appsの物理入力・表示・音声・終了後FE復帰は実機acceptanceが必要。
  - 2026-08-15: 実機backend監査でScraping plan、File Manager、Music Player、RetroArch RGUI、Pyxel Setup、PortMasterを合格。RetroArch Appのudev準備漏れ、FE stop/launch helper欠落、zombie誤認を`85fffad`で修正。Update PortMasterのnetwork installと7 AppsのFE物理選択は継続。
  - 2026-08-15: File ManagerのMF button order誤流用と、回転rendererが論理640幅を物理480幅でclipする不具合を`9b4070d`、`0106a75`で修正。署名Runtime、全幅DRM capture、event2経由のD-pad/A/B/FUNCTION/Quit、FE再取得に合格。実物buttonのoperator目視は継続。
  - 2026-08-15: File Managerが外部commandを`execvp("cp")`等で起動する一方、Pixel2 Systemには個別の`/bin/cp`等が無く、copy/move/link/rename/delete/mkdirが失敗していた。`883fd1d`でPixel2だけBusyBox本体へapplet名を渡す実行方式に変更し、複数選択progress判定も修正。署名Runtime `0.1.0-dev-883fd1d`を適用し、実際のX/A操作によるcopy、コピー元/先SHA-256一致、FE復帰に合格。
  - 2026-08-15: Music PlayerがLinuxの`BTN_A=304`、`BTN_B=305`を物理labelとして扱い、Pixel2の物理A=305/B=304と逆転していた。FUNCTION=704も未処理だった。`b370bfa`でA=再生、B/FUNCTION=終了へ修正し、D-pad、A/B/X/Y、START/SELECT、L/R、FUNCTIONの実機EV_KEY経路、3曲遷移、音声開始、clean exit、FE復帰に合格。
  - 2026-08-17: V90S/XU20/MFのPortMaster履歴を基に不足ABI、audio、UINPUT、KMS handoffを補完。RTL8821CU Wi-Fi上で公式catalogを更新し、Ready-to-Run OpenSyobonをinstall。共通SDL interposerで640x480 logical viewをPixel2の480x640 scanoutへ正しく回転し、GUI/game capture、ALSA RUNNING、gptokeyb UINPUT、終了後FE復帰、Wi-Fi維持、mutable install保持、実機root checksumに合格。詳細は`docs/validation/2026-08-17-pixel2-portmaster-ports.md`。
  - 2026-08-18: FEのPorts routeが`/roms`と`/mnt/plumos-user/roms`を文字列比較して全拒否する問題、SDL dialog/LOVE実行権限/停止側aliasを修正。Balatro購入データをROM setからSHA一致で保持し、adapter 29の共通SDL/OpenGL interposerでLÖVEの論理640x480を物理480x640へ回転。正式Runtime `0.1.0-dev-7b0d69f`上のFE `external:port`経路でdisplay setupとpatcherを正立4:3 capture済み。patcherの物理A確認、`Balatro_pm`生成、ゲーム内操作・音声はoperator確認を継続。
  - 2026-08-18: SDL portが`SDL_GetCurrentDisplayMode()`からpanel-native
    480x640を取得し、回転後も内部layoutだけportraitのままになる欠陥をApotrisで
    特定。adapter 30で論理640x480へ補正し、ApotrisのHOLD/盤面/NEXTが画面内へ
    収まること、OpenSyobonの既存46px side bar、終了後FE復帰を実機合格とした。
    desktop/enumerated modeまで補正したことでBalatro patcherのKMSDRM mode選択を
    壊す回帰が判明したため、adapter 31ではapplication layout用のcurrent modeだけを
    補正する。署名Runtime `0.1.0-dev-c55da25`、`runtime_healthy`、root checksum、
    FE相当Ports経路のBalatro patcher正立表示まで実機合格。物理Aでのpatch完了、
    `Balatro_pm`生成、ゲーム内操作・音声はoperator確認を継続。
  - 2026-08-18: patcherの物理Aは正常に受理され、購入元を保持したまま
    `Balatro_pm`を生成していた。adapter 33で共通patcherの16 px文字を24 px、
    dialogを128 pxへ拡大。ゲーム本体だけが落ちる原因を、GL回転用論理FBOの
    depth/stencil欠落と切り分け、adapter 34で24-bit depth + 8-bit stencilを追加した。
    署名Runtime `0.1.0-dev-a64fde3`はhealth昇格、全Runtime checksum、再起動後保持に
    合格し、通常Ports導線のBalatroタイトル正立表示と継続稼働をDRM captureで確認。
    ゲーム内の物理操作と実音声はoperator確認を継続する。
  - 2026-08-18: Balatroの無音をOpenALが`default` PCMを開いたまま
    `PREPARED`（`hw_ptr=0`, `appl_ptr=0`）で停止する問題と確定。V90Sの実績どおり
    `default`を直接`plumos_hotplug`へ接続し、PortMasterではPixel2独自poll proxyを
    無効にして実PCM descriptorを使う。adapter 35はLÖVEのstencil passが残す
    color-write maskもpresent前後で退避・復元する。署名Runtime
    `0.1.0-dev-651e557`は再起動後もhealthyで、Balatro PCM 50/50回RUNNING、
    hardware pointer連続進行、DRM 40/40 frameの黒画面なし、managed checksum、
    購入元・生成物・build stamp保持まで合格。実際の可聴音とLCD点滅はoperator確認を
    継続する。
  - 2026-08-18: adapter 37で回転full-screen drawとKMS/SDL swapの間に
    `glFinish()`を追加。3枚のKMS scanoutが完全frameで、swap intervalも1である
    ことを切り分けた上で、Mali/Panfrostの完了境界不足と確定した。operatorが
    Balatroの移動・アニメーション時の点滅解消を確認。署名Runtime
    `0.1.0-dev-3480628`を明示health昇格し、安全再起動後の保持、全checksum、
    adapter 37、音声`RUNNING`、終了後FE復帰、購入データ保持まで合格した。
  - 2026-08-20: PortMasterのcolor scheme変更後にGUIが再起動せず、以後のFE起動も
    即終了する問題を公式`PortMaster.sh`との差分から特定。Pixel2はPython bootstrapを
    直接1回だけ起動していたため、`.pugwash-reboot`が残るとupstream `pugwash`が
    `pm.run()`をskipしていた。adapter 38で起動前のstale marker消費と、変更直後の
    同一session内再起動を公式契約へ揃え、異常な再生成だけ8回で打ち切る。host testは
    合格し、実機の`default_theme / Dark Mode`設定を保持したmarker recoveryも完了。
    signed Runtime `0.1.0-dev-7e6b734`を適用し、`runtime_healthy`、adapter 38、
    component 172件とRuntime 4,261件のchecksum、同じ`.110`へのWi-Fi cold-boot復帰、
    FE handoff後のGUI process継続と`Dark Mode` theme loadまで確認した。operatorによる
    LCD表示、scheme再変更時のin-place再起動、通常終了後のFE復帰を最終確認する。
- [ ] GitHub release readinessを実装する
  - [x] top-level English READMEを追加し、日本語READMEを現行boot/image構成へ同期する
  - [x] user/developer文書を分離し、主要取扱説明書と技術ガイドを英語・日本語の対にする
  - [x] top-level project licenseを他plumOSシリーズと同じMITとして追加する
  - [x] third-party noticeとDraStic redistribution条件をrelease payload単位で監査する
  - [x] CIへhost tests、identity/content gate、implementation audit、checksum検証を追加する
  - [x] versioned artifact、SHA256SUMS、archive検査、GitHub再download検証を実装する
    - `prepare-pixel2-release.sh`はclean commitから全component、strict app/System/image、
      同条件image再生成、他plumOSシリーズと同じ`.7z`のarchive testと`.img` round-trip、
      source archive、manifest、checksumsを生成し、
      Git tag・GitHub Release・uploadは行わない。
    - `verify-pixel2-release-bundle.py --download-base`は公開後の全asset再downloadと
      checksum・展開image・source archive検証を行う。
  - [ ] exact release bundleを別SDへ書き込み、cold boot・初回setup・実機最終確認後に
    GitHubへ公開し、公開assetの再download検証を完了する

## Build system and app layer

- [x] Pixel2共通Docker entry pointとcomponent targetを実装する
- [ ] V90S実績のRTL8811CU/RTL8821CU USB Wi-FiをPixel2でrelease acceptanceする
  - [x] V90Sと同じ`8821cu` source系列をstock 5.10.198 ABI向けにpinned buildし、stock `r8188eu` srcversion/vermagic照合、module alias、license、provenance、System checksumへ統合する
  - [x] `0bda:1a2b` driver diskのbounded eject、`0bda:c811`再列挙、`8821cu` load、scan継続をhost fixtureで検証する
  - [x] stock moduleの`__this_module` size/exit relocationをbuild gate化し、実機5.10.198でSHA一致moduleの`insmod`/`rmmod`を確認する
  - [x] signed inactive-slot System `0.1.0-dev-13ad915`を適用し、slot B readback、health promotion、収録module checksum、`modprobe`/`rmmod`、FE/ADB復帰を確認する
  - [ ] 実adapterで1a2b->c811、2.4/5 GHz、DHCP、SSH/SFTP速度、抜き差し、cold boot復元を確認する
    - [x] UGREEN AC650実adapterで`0bda:1a2b`、`sr0`、SCSI eject要求、
      `0bda:c811`再列挙、`8821cu`、2.4 GHz `k-home-2`、DHCP
      `192.168.10.107`、gateway 20/20 ping、24,337,159-byte SFTP往復SHA一致を確認した。
      RX/TX errorは0。eject後のUSB disconnectによりutil-linuxが非0を返しても、実際の
      `c811`を成功判定にする修正を追加。5 GHz・物理再挿入・cold bootは継続する。
      通常rebootでは11秒で`wlan0`まで到達したが、初回associationが15秒上限を超え、
      全add eventがcoalesce済みのためDHCPなしで停止することを確認。物理再挿入だけに
      依存しないよう、boot/sync失敗時だけ5秒後に1回再試行するbounded recoveryを追加した。
      Runtime `875227e`適用・全11,267管理fileのchecksum合格後、UGREENを挿したままの
      通常rebootでuptime 10.44秒に`c811`、11.09秒にdriver、31.39秒に`.107`を自動取得。
      host SSHは再起動要求から37秒で復帰し、gateway 10/10、FE、RX/TX error 0を確認した。
      物理再挿入復旧も`.107`再取得まで合格。5 GHzとtrue cold bootは継続する。
    - [x] release candidateを別々の2枚のSDで初期化した際、初回のSSID接続だけ
      WPA associationが固定15秒上限をわずかに超えて失敗し、失敗候補が残留して
      2回目だけ成功しやすくなる競合を再現した。明示的なFE接続だけWPA待機を30秒へ
      延長し、保存済み設定がないfresh cardでも失敗候補を必ず停止する。host fixture、
      frontend/app-layer strict build、実機Runtime checksumを合格。実機では保存設定を
      変更せず18秒で`.107`へ初回接続でき、旧15秒経路なら失敗する境界を通過した。
      詳細は`docs/validation/2026-08-23-pixel2-first-wifi-connect.md`に記録した。
    - [x] `0bda:c820`実adapterを`rtl8821cu`へ直接bindし、2.4 GHz接続、DHCP
      `192.168.10.120`、gateway 20/20 ping、SSH/SFTP SHA一致、5 GHz scanを確認した
    - [x] 5 GHzへassociationし、434 Mbit/s link、DHCP/gateway、SSH 2.11 MiB/s、
      FTP 2.19--3.22 MiB/s、SFTP下り3.69 MiB/sを確認した
    - [x] SFTP上りだけが5 GHzでも0.32--0.65 MiB/sに留まり、4 MiBでTCP retrans
      +22/timeout +4となる既知制約を受理する。tmpfs、request数、省電力OFF、driver/AP、
      FTPとの比較まで実施して改善策がなく、SSH/FTPとSFTP下りは実用速度のため追加調査を
      完了する。採用範囲は`package/frontend-pixel2/deferred-scope.json`に固定した。
    - [x] Pixel2で欠落していたV90S方式のkernel uevent monitorとbounded recoveryを
      移植し、wlan add/1a2b/c811/c820 add、Wi-Fi OFF抑止、PID検証をhost testで確認した
      - 2026-08-17: direct `c820`追加を監視対象に含めていなかったため、抜き差し後は
        手動OFF/ONまでmoduleがloadされなかった。Runtime `42bcb46`の実機ログで原因を
        確定し、direct aliasもbounded recoveryへ接続した。物理再試験は継続する。
      - 2026-08-17: V90S `138514a`のcold-boot contractに対して、Pixel2はFE起動時の
        `recovery sync`を欠き、早すぎる一回限りの独自boot callだけになっていた。
        boot/FEの両方をmonitor + initial recoveryへ統一。署名System実機試験は継続する。
      - 2026-08-17: ADBからdirect `c820`へ差し替えるだけで5 GHz `k-home-1`、DHCP
        `192.168.10.120`、全network serviceが自動復帰した。USB/net addのqueueにより
        接続後も4回再実行されたため、接続済みevent抑止とcold boot再試験を継続する。
      - 2026-08-17: dongle接続状態のrebootではLED消灯・SSH未復帰を再現。Wi-Fiより先に
        起動するADBが保存ONだけを見て唯一のOTG portをdevice roleへ強制していた。
        upstream VBUSなしではADB intentを保持したままhost roleにするarbiterを追加した。
      - 2026-08-17: arbiter適用後のrebootではADBはhost待機へ正しく移行したが、DWC2が
        挿しっぱなしの`c820`を列挙せず、物理抜き差し時のuptime 733秒で初めて現れた。
        実機上で`ff300000.usb`だけをunbind/bindすると1秒以内に再列挙し、`.110`へ自動
        復帰した。Wi-Fi保存ON、upstream VBUSなし、downstream未列挙時だけ同じresetを
        非同期実行するSystem serviceを追加し、signed Systemでのreboot再試験を継続する。
      - 2026-08-17: signed System `49a4f15`はslot Aでhealthyになり、serviceもuptime
        5.5--7.6秒でDWC2 resetを実行したが、VBUSが切れず物理抜き差しまで列挙されなかった。
        stock DTBで`vbus-supply`が欠落しDWC2がdummy regulatorを使うことを確定。stock DTBを
        inputに既存RK817 `OTG_SWITCH`への1 propertyだけを追加し、他差分を拒否する生成gateを
        追加した。stock kernel/initramfsと`dr_mode=otg`は維持し、実機boot/充電rebootを再検証する。
      - 2026-08-17: patched runtime DTBを実機へreadback検証付きで配備し、stock DTBは
        `/flash/rk3326s-gkd-pixel2.dtb.stock-a7a438f7`へ退避した。8821CUを挿したまま再起動し、
        物理抜き差しなしでuptime 2.05秒に`0bda:c820`、9.38秒に`8821cu`、10.25秒に
        `wlan0`、保存SSIDの`.110`へ復帰した。Wi-Fi saved-ON reboot gateは合格し、
        true power-off cold bootは引き続き実機gateとする。
      - 2026-08-17: 8821CUを挿したまま完全電源OFFからcold bootし、uptime 2.05秒で
        `0bda:c820`、8.97秒で`8821cu`、10.07秒で`wlan0`を確認。保存済み5 GHz
        `k-home-1`へ`192.168.10.110`で自動接続し、gateway 5/5、RX/TX error 0。
        tested direct-`c820` adapterのsaved-Wi-Fi cold-boot gateは合格。
      - 2026-08-19: ADB回帰の境界を切り分けるため、DWC2 `vbus-supply`追加を撤回し、
        runtime DTBをchecksum登録済みstock DTBとのbyte-for-byte一致へ戻した。image builder、
        manifest、readback verifier、現行architecture docsをexact-stock契約へ更新。まずADB
        cold boot、実`adb shell`、物理抜き差しを合格させ、その後同じstock DTBのまま
        RTL8821CU列挙、保存SSID、DHCP、SSH、cold bootを確認する。stock DTBでWi-Fiの追加
        VBUS操作が必要なら、DTBを再変更せずstockOS userlandの制御手順を採取して移植する。
      - 2026-08-19: exact-stock DTB bootでもADBは`waiting`。SDから採取した永続logでは
        cable接続時にPHYのID/BVALID/USB onlineはdevice条件へ遷移する一方、DWC2は
        `is_a_peripheral=0`、UDC `not attached`のhost modeへ残留していた。stock DTBには
        `usb-role-switch`がなくsysfs role書込みは実行されていなかったため、DTB/kernelを
        変更せず、ADB start時だけstock DWC2 driverと同じ`GUSBCFG.FORCEDEVMODE`を一度設定し、
        stop時は両force bitを解除してstock OTG自動判定へ戻すhelperを追加。常駐監視なしで
        cold boot、実`adb shell`、抜き差し、ADB -> Wi-Fiを実機再検証する。
    - [ ] `1a2b -> c811`実adapter、物理抜き差し、cold boot後の保存接続を確認する
      - 2026-08-23: V90Sのmode-switch処理は移植済みだったが、Pixel2 Systemに実行時
        `eject`が未収録と判明。`/usr/bin/eject`とlicenseをSystemへ追加し、rootfs verifierを
        必須化。1a2b remove後のOTG解放もV90Sの5秒待ちと競合しない8秒へ延長した。
        host fixture・System生成物検証後、UGREEN実adapterでのacceptanceを行う。
- [x] frontendをSystemからapp-layer componentへ分離する
- [x] `plumos-text-ui`とPixel2 launcher lifecycleを統合する
- [x] Pixel2向けRetroArchをpinned sourceからbuildする
- [x] QuickNESをpinned sourceからbuildしcomponent manifest/checksumを生成する
- [x] Pixel2向けALSA `plumos_output` audio-router componentを実装する
- [x] strict app-layer assemblerとmanaged/mutable path gateを実装する
- [x] app-layerをseedしたext4 filesystemをSD image buildへ統合する
- [x] canonical libretro core recipe catalogとfilter buildを実装する
- [x] 従来mGBAと新しいmGBA Modernを上書きせず両立する
  - 2026-08-26: 従来版`4f70b313`の`mgba_libretro.so`と既定profileを維持し、
    新版`e31759b2`を`mgba_modern_libretro.so`、内部名`mGBA Modern`として追加。
    GB/GBC/GBAのRA core選択へ追加し、通常saveは共有、savestateは新版専用領域へ分離した。
    AArch64 target build、component manifest/checksum、Color CorrectionとInterframe Blendingの
    binary option keyをhost確認済み。2026-08-27にRuntime deltaで実機へ導入し、旧coreの
    byte一致維持、新旧manifest登録、全component checksum、更新後health、FE core導線を確認。
    実機での新旧性能比較と画面効果の目視確認は継続する。
- [x] RAのCore Provided aspectを通常coreで選択・保存できるようにする
  - 2026-08-27: RA main cfgには`aspect_ratio_index=22`が保存されていたが、Pixel2 launcherの
    高優先度append cfgが毎回`0`へ戻していた。通常coreではaspectをappendせず、GLESのFullと
    WonderSwanのCore Providedだけ画面経路契約として限定上書きするよう修正。実機captureで
    RAとDRMの90度回転によるcore geometry二重反転も検出し、DRM側でCore Providedだけ相殺。
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
- [x] standalone emulator binaryをPixel2向けに順次build・実機検証する
  - 2026-08-12: OpenBOR standaloneをPixel2向けにsource buildし、`standalone/openbor/bin/OpenBOR`、runtime deps、license、component checksumへ統合。ROM set route validationでopenborは`ok`へ移行。実機起動・入力・画面向き・終了hotkeyは未検証。
  - 2026-08-13: N64のMupen64Plus-Nextはdynarec無効、cached/pure interpreter、GLideN64/Angrylion/ParaLLElの全実機試験でsegfaultしたため、壊れたalternateを残さずParallel N64だけを公開する。
  - [x] 2026-08-28: standalone Mupen64Plus 2.6.0を追加選択肢として移植する。pinned 6 component、Rice GLES2、Pixel2最終回転、`pixel2_joypad`、FUNCTION終了、plumOS音声経路を実装。`d4ab428`の署名Runtimeで起動、論理640x480・4:3、物理480x640回転、ALSA進行、FE復帰、Runtime checksumを機械合格。Riceの可変DBはlaunch-private data stagingで管理app-layerから分離した。風来のシレン2で発見したunaligned SRAM部分書き込み欠落は`eb5499a`で4 byte境界保存へ修正し、署名Runtime `0.1.2-dev-eb5499a`上で新規セーブ、FUNCTION相当の正常終了、再起動、セーブ読込、続きからの開始、FE復帰を実機合格。既定`retroarch:parallel_n64`は維持する。[検証記録](docs/validation/2026-08-28-pixel2-mupen64plus-standalone.ja.md)
    - 2026-08-29: アナログstickを持たないPixel2向けに、物理十字を通常N64 analog、FUNCTION短押しをN64十字／analog切替、1.5秒長押しを終了とする救済経路を実装。SDL button 14がMupen input pluginへ届かない実機差を`96f3af5`でraw evdev helperとPID固有`/run` markerの連携へ変更し、200ms release debounceを維持した。署名Runtime `0.1.2-dev-96f3af5`はhealthy、standalone 640/640 checksum合格。時のオカリナで短押し切替と再切替を実機合格。
  - 2026-08-12: Nintendo DSはPixel2向けDraStic standaloneを追加。armhf DraStic core、source-built Pixel2 integration library、package-local armhf runtime、armhf ALSA `plumos_output` pluginをapp-layerへ統合し、FE routeを`standalone:drastic`へ固定。DraStic BIOSは配布物へ含めず、実機では`/mnt/plumos-user/bios/drastic`、`/mnt/plumos-user/bios/nds`、`/mnt/plumos-user/bios`からmutable workdirへ取り込む。
  - 2026-08-12: PPSSPP v1.20.4をpinned source buildし、Pixel2向けSDL2/GLES/EGL binary、assets、factory `ppsspp.ini`/`controls.ini`、manifest/checksumへ統合。ROM set route validationでPSP `standalone:ppsspp`は`ok`へ移行し、代表ROMがある29 systemのpending binaryは0。実機での画面向き・入力・音声・終了hotkeyは未検証。
  - 2026-08-14: PPSSPPの縦画面とFunction無反応を実機再現。Pixel2の480x640 panelを論理640x480からCCW回転するpresenterと、実測した15-button mapping（Function=SDL Guide button 14）をsource buildへ統合。`612822f`でPSP比率補正0.5625とUI scale -2へ移行し、署名Runtime `0.1.0-dev-612822f`のhealth昇格、DRM実画面640x363、物理Functionでの拡大pause menu表示を確認。オペレータも画面表示と文字サイズを合格判定。
  - 2026-08-14: `Telegraph Crosswords.cso`のDRM primary planeを直接captureし、ゲーム領域が640x204（約3.14:1）へ潰れることを確認。同じportrait panelのA30で実績あるLandscape補正`0.562500`により640x363（PSP native約1.76:1）へ修正し、UI scaleも`-8`から`-2`へ拡大。正式Runtime適用後のゲーム画面とpause menuのDRM capture、物理Function操作、FE復帰まで合格。
  - [x] PICO-8 official standalone
    - [x] `roms/pico-8/aarch64`と`roms/pico8/aarch64`からuser-supplied runtimeを検出し、ELF machineがAArch64のbinaryだけを選択する
    - [x] FE既定SA導線、mutable home/cdata分離、Pixel2 controller mapping、ALSA `plumos_output`、GLES2 SDL回転adapter、bounded stop ownershipを実装する
    - [x] commit `529f78f`のstrict app-layerとrelease gateを合格させ、実機Runtime `0.1.0-dev-529f78f`へcomponent/root checksum整合で反映する
    - [x] `Celeste.p8`のKMSDRM/GLES2起動、controller 1台15 button認識、`SDL_OpenAudio ok`、cart load、DRM captureの640x480正方向・正方形表示を確認する
    - [x] 既定のinteger 3x（384x384）をaspect-fit（480x480）へ拡大し、左右80pxの黒帯で1:1比率が維持されることを`X-Zero.p8`のDRM比較captureで確認。operatorも拡大表示を合格判定
    - [x] 物理D-pad/ABXY/Start pause、実音声、終了、FE復帰をoperator確認する
  - [x] PCSX-ReARMed
    - [x] pinned sourceからAArch64 binaryとpackage-local sdl12-compatをbuildし、component manifest/checksumへ統合する
    - [x] Pixel2の480x640 framebufferへCCW回転した論理640x480/4:3 presenter、物理button/hat、`plumos_output` 48 kHz routeを実装する
    - [x] clean `8b54b97` app-layer、署名Runtime差分、実機health promotion、3468 root checksumを検証する
    - [x] ROM setの`PSX/chroQW.img`を実機へSHA一致で配置し、`standalone:pcsx_rearmed` launch planを解決する
    - [x] 代表PSX ROMで実機acceptanceを完了する
  - [x] ScummVM standaloneは保留し、実装済みlibretro routeをPixel2採用経路とする
  - [x] EasyRPG standaloneは保留し、実装済みlibretro routeをPixel2採用経路とする
  - [x] Flycast standaloneは保留し、実装済みlibretro routeをPixel2採用経路とする
  - [x] NXEngine-Evo standaloneは保留し、実装済みlibretro routeをPixel2採用経路とする
- [x] Docker runtime復旧後、clean commitからPicoArch/standalone統合済みapp-layerでSD imageを再生成する
  - 2026-08-12: 4 GiB seed layout（p1=512 MiB, p2=2048 MiB, p3=remainder）でdirty-tree生成は完了し、host checksum/MBR/hdiutil partition recognitionは確認済みだったが、Docker Desktopがcontainer metadata I/O error後にAPI socketを失ったため、clean commit source_refでの再生成が必要になった。
  - 2026-08-12: Docker Desktopをforce stop/startで復旧し、clean commit `631c30b` から `./scripts/docker-build.sh sd-image` を再実行。`verify-sd-image.sh` のSYSTEM/app-layer/filesystem検証まで `sd_image=result-ok`、host checksumもOK。

## Frontend game lifecycle

- [x] FE catalogは存在するruntime/coreだけを公開する
- [x] NES ROM scan -> `retroarch:quicknes` launch planをhost検証する
- [x] Pixel2 live launcherで`PLUMOS_ROM_ROOT=/roms`からNESがRetroArchへ到達することをADB検証する
- [x] Pixel2 RetroArchが`pixel2_joypad`をport 1へautoconfigすることをADB検証する
- [x] FEがDRM/inputを解放し、emulator終了後に再取得することを実機確認する
  - 2026-08-15: `plumos-frontend-stop/launch`を追加し、RetroArch RGUI起動前の解放、終了後のFE/hardware-key daemon再取得をADBで確認。物理FEからの全emulator終了acceptanceは継続。
- [x] RA/PicoArch/SAの物理Function menuを実機確認する
  - 2026-08-13: RA、PicoArch、PCSX-ReARMed、DraStic、PPSSPP、OpenBORのFunction menu契約を実装し、source contract testを追加。`e9c8f38`から全対象をbuildし、署名Runtimeを実機へ適用。health昇格、対象22 SHA一致、root checksum 3470件合格。各runtimeの物理menu/exit確認が必要。
  - 2026-08-13: PCSX内蔵menuでevdevとSDL joystickが同じ`event2`を二重登録する状態を実機FDで確認。Pixel2のPCSXはraw evdevだけをcontroller入力元とする`002e250`へ修正し、全4 SAを並列build、署名Runtime `0.1.0-dev-002e250`を適用。health昇格、実機root checksum 3470件/失敗0。PCSX menuの十字/A決定/B戻るは物理再確認待ち。
  - 2026-08-13: `3234b0d`でPCSX menuの物理Function、十字、A決定、B戻るを実機合格。RA、PicoArch、DraStic、PPSSPP、OpenBORは引き続き個別物理確認が必要。
  - 2026-08-16: PicoArchはFunctionから内蔵menuを表示し、`d1f5ea1`適用後に
    game/menuの十字操作とsleep復帰を実機合格。RA、DraStic、PPSSPP、OpenBORの
    個別menu/exit確認は継続する。
  - 2026-08-22: PicoArch + FBNeoだけFunction menuが開かない原因を、FBNeoの
    `retro_run()`がlibretro必須の`input_poll_cb()`を呼ばない実装に特定。正常coreは
    従来どおりcore callbackでpollし、省略coreだけ各frame終了時にfrontendが補完pollする
    PicoArch host fallbackを実装。AArch64正規build、component checksum、Function契約と
    app-layer host testに合格。正式Runtime反映後の物理Function menu確認を継続する。
- [x] RetroArch factory configuration一式をPixel2へ実装する
  - 2026-08-15: 3,374 unique keyをPixel2 DRM/udev/button/audio/storage contractへ適合。旧57-key factoryは既知SHA一致時だけatomic置換し、変更済みuser cfgには不足keyだけを補完するmigrationを追加。署名Runtime `0.1.0-dev-aa3a3ab`を適用し、healthy、root 4241/4241 checksum、RGUIのDRM/event2取得、FE復帰を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-v90s-config-port.md)
  - 2026-08-15: 前記移植がmain cfgだけで、content-local save/state、L2/R2 hotkey変換、core-options、N64 remapを欠いていたことを訂正。`68abe6c`で3-file factory bundleと旧世代12項目の限定migrationを実装し、署名Runtime `0.1.0-dev-68abe6c`を適用。healthy、Frontend 191/191、RetroArch 59/59、root 4245/4245、既存state 2件のSHA不変を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-save-hotkeys.md)
  - 2026-08-15: `68abe6c`がPixel2で動作確認済みのSTART+SELECT直接終了をV90Sのmenu comboへ誤って変更した回帰を確認。`612cc19`で`enable=SELECT(8)`+`exit=START(9)`を復元し、FUNCTION menu(14)を維持。旧Runtimeの2値だけを直すmigrationを署名Runtimeへ適用し、既存fallback/content-local state 6件のSHA不変を確認。
  - 2026-08-15: plain DRM OSDが物理480x640座標へ未回転描画され、pending messageもthread間で無保護だった。`70357bb`で論理640x480からの回転描画、mutex、glyph bounds、font既定値と限定migrationを実装。署名Runtime上でsave slot 8通知の正方向・全文表示、RA継続動作、RetroArch 59/59とFrontend 191/191 checksum、試験設定のbyte一致復元を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-osd.md)
  - 2026-08-15: 物理実機で動作確認済みのmutable `retroarch.cfg`をbyte-for-byteでfactory/build defaultへ採用（3,376 unique key、SHA-256 `231ee258...`）。`72f42e5`から同versionでRetroArch、Frontend、strict app-layerを再buildし、署名Runtime `0.1.0-dev-72f42e5`を適用。active/factory/factory-reset用cfgの同一性、既存active cfg不変のmarker移行、RetroArch 59/59、Frontend 191/191、FE/ADB稼働を確認。[検証記録](docs/validation/2026-08-15-pixel2-retroarch-live-default.md)
  - [x] RGUI/GLUI/Ozone/XMBと多言語切替をmanaged assets込みで実機確認する
    - 2026-08-21: 4 menu driverと追加言語、公式managed assetsを実装。Pixel2の
      native 480x640 GL viewportとlogical 640x480 menu layoutの差をlayout、font
      size、font positionの3経路で補正し、日本語XMBの文字/icon重なりを解消した。
      GLUI、Ozone、XMBを最終Runtime `0.1.0-dev-9e580ec`のDRM captureで合格、
      RGUI/Englishの元設定とhotkeyを復元し、root 11266/11266 checksum、FE復帰を確認。
      [検証記録](docs/validation/2026-08-21-pixel2-retroarch-menu-localization.md)
    - 2026-08-28: game launcherが保存済み`menu_driver`を見ず、software coreをplain DRMへ
      固定していたため、AppsではOzone/XMB/GLUIを選べてもgame中はRGUIへ戻る経路差を修正。
      graphical menu選択時だけGLESへ切り替え、private cursor/icon行列にもPixel2固定回転を
      反映した。Ozone/XMB/GLUIのDRM capture、通常NES起動、署名Runtime
      `0.1.2-dev-6581c54`のhealth昇格と再起動後検証に合格。
      [検証記録](docs/validation/2026-08-28-pixel2-retroarch-menu-selection.ja.md)
- [x] Pixel2 RetroArch video rotation/scalingとframe pacingを実機確認する
  - 2026-08-14: WonderSwan `Puzzle Bobble.ws` (`mednafen_wswan`)でSELECTによる縦/横切替後の表示が180度逆さになることをDRM overlay planeのRGB565 captureで確認。`465b957`でWonderSwan系のみ`video_allow_rotate=false`とし、core内content回転後にPixel2固定panel補正を適用。署名Runtime `0.1.0-dev-465b957`のhealth昇格と縦向き正方向captureは合格。SELECT切替後の横向き物理captureは継続。
  - 2026-08-14: 回転修正後に共通4:3固定でWonderSwan映像が伸長されることを実機確認。`e327fb9`でcore-provided aspectへ切り替えたが、SELECT後の144x224 frameへPixel2固定回転分のaspect反転が二重適用され、物理方向640x411になる誤りを目視指摘で再確認。`f7bd277`で`ASPECT_RATIO_CORE`だけDRM側の重複反転を相殺し、物理SELECT 1回後の正式Runtime captureを309x480（144:224と丸め誤差内で一致）へ修正。署名Runtime `0.1.0-dev-f7bd277`はhealth昇格済み。最終LCD目視確認は継続。
  - 2026-08-14: `5c99bd9`でWonderSwanのcontent回転とPixel2固定panel回転を分離。`video_rotation=0`、core software rotation、core-provided aspect、最終`PLUMOS_DRM_PANEL_ROTATION=3`とし、拒否したcore rotationをfrontend aspectへ残さない。物理SELECT後の144x224 / 9:14、正方向表示をDRM captureと実機目視で合格。署名Runtime `0.1.0-dev-5c99bd9`はhealth昇格、managed SHA一致、3490/3490 checksum合格。
  - 2026-08-16: DreamcastのFlycast XtremeをKMS/GBM `gl`へ移し、GL用`video_rotation=1`、Dreamcastのcore rotation拒否、Full aspectで正立640x480全面表示へ修正。続いてRGUIだけが無回転行列で描画される問題を`f46740d`でDreamcast限定のcontent行列へ修正。署名Runtime `0.1.0-dev-f46740d`はhealthy、Frontend 194/194、RetroArch 59/59、PicoArch 11/11、root 4248/4248に合格。最終DRM captureと物理Functionで開いたメニューの実LCD方向をoperator確認済み。[検証記録](docs/validation/2026-08-16-pixel2-dreamcast-display.md)
  - 2026-08-16: app-layer manifestのGLES必須4 core（Flycast 2種、ParaLLEl N64、DuckSwanStation）を監査。N64だけ`drm`へ漏れて逆さま・3:4表示だったため、`2944596`で全hardware-GLES coreを`gl`、rotation 1、Full aspect、content-matrix RGUIへ統一し、manifestに追加されたGLES coreのlauncher漏れをrelease gate化。署名Runtime `0.1.0-dev-2944596`はhealthy、Frontend 194/194、RetroArch 59/59、cores 357/357、root 4248/4248に合格。N64、DuckSwan、通常Flycastの最終DRM game captureとN64/DuckSwanのRGUI captureは正立640x480。さらに正式Runtime上で物理Functionから開いたN64メニューの実LCD方向をoperator確認し、最終XR24 plane captureも合格。[検証記録](docs/validation/2026-08-16-pixel2-gles-core-audit.md)
- [x] `plumos_output`経由のaudio、D-pad、ABXY、START/SELECT、shoulder、終了hotkeyを実機確認する
- [x] save/stateが再起動後も保持されることを実機確認する
  - 2026-08-15: content-local save/state、自動exit state、10秒autosave、20世代state、thumbnailをfactoryで有効化。更新前から存在したfallback state 2件は更新・再起動後もSHA一致。ゲーム内で新規save/stateを書き、再起動後にloadする物理acceptanceは継続。
  - 2026-08-23: エミュレータ全体の最終操作、終了、FE復帰、save/state保持をoperator合格とした。
- [ ] enabled systemのBIOS/firmware inventoryを完備する
  - [x] 有効routeのlibretro `.info`とstandalone要求からPixel2 BIOS staging/manifestを生成する
  - [x] ROMセットに無い`ecwolf.pk3`と`kick34005.CDTV`は入手元がないため保留し、
    配布物へ同梱せず、取得できた場合だけmerge-onlyで追加する方針を確定する
  - [x] ROMセットに存在するBlueMSX archiveを含む486 firmware fileを実機`/mnt/plumos-user/bios`へmerge-only配置する
  - [x] fresh SDへfirmware fileをmerge-only復元し、manifest照合を合格させる
    - 2026-08-22: 現在の実機をホストstaging manifestでread-only照合すると0/486で、
      過去に配置したBIOSがfresh SDへ継承されていない。Channel F/ColecoVision失敗と整合。
      競合0で486件を復元後、Analogue Pocketの既知`cfbios.bin`からMD5検証付きでChannel F
      BIOS 2件を分割し、最終488/488 SHA一致。missing requiredは5から2へ減少。
  - [ ] enabled systemごとにBIOS検出と代表content起動を実機確認する
- [x] enabled 87 systemのcontent policyを確定する
  - 2026-08-22: 代表ROMがない33 systemは現時点で規約を推測せず保留とし、
    libretro routeと既存FE導線を維持する。ROM入手時に再開する受理済み保留として
    `package/frontend-pixel2/deferred-scope.json`へ固定した。
  - [x] arcade ROM set policy: arcade、cps1、cps2、cps3、fbneo、neogeo
  - [x] disk image policy: amiga、atari800、atarist、c64、cpc、pc88、pc98、sharpx1、thomson、vic20、x68000、zx81、zxspectrum
  - [x] data layout policy: cannonball、cavestory、chailove、dinothawr、lowresnx、lutro、microw8、quake、wolf3d
  - [x] frontend policy: j2me、music、ti83、vmu
  - [x] scraper source policy: uzebox
- [x] disabled 9 system（saturn、mame2003plus、2048、bk、daphne、flashback、mrboom、palm、rickdangerous）を実装または非対応理由確定する
  - [x] Saturnは`unsupported_performance_rk3326`として非対応理由を確定する
  - [x] 残る8 systemはRK3326性能・Pixel2製品範囲を踏まえて非採用とし、FEで無効のまま維持する

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
- [x] compact seed imageとfirst-boot後partitionをhost/実機検証する
  - 2026-08-14: 現行64 GB実機SDで最終境界（p2=8192 MiB、p3=残り49.7 GiB）、既存user data復元、cold boot mountには合格。この時点ではrelease seed側が未実装だったため項目を継続した。
  - 2026-08-14: release seed provisionerを実装し、16 GB sparse-cardで最終MBR、ext4 8 GiB、残容量FAT32、directory seed、中断resume、idempotency、既存p3保護までhost合格。新規compact imageからの物理Pixel2初回bootのみ継続。
  - 2026-08-14: `e1b05ed` compact imageを64 GB実機SDで初回bootし、約1秒でp2=8 GiB、p3=49.7 GiB、complete/userdata markerまで完了。旧p3 FAT32 signatureをpreserveしてformatを省略したため短時間だった。初回sessionだけprovisioner mountへinitがfallback tmpfsを重ねる不具合を検出し、通常再起動後はp3 49.7 GiB、required directory 10/10、FE/ADB、mount count 1に合格。`cf8e96f`で既存mount採用guardを追加し、修正版imageの初回session確認を継続。
  - 2026-08-23: release candidate `0.1.0-rc1-28aaf65`を全componentから再生成。
    16 GB first-boot fixture、System A/B、Runtime再展開、固定partition geometry、2回の
    image SHA再現性に合格。新規SDでのfirst-bootと最終partition readbackをrelease gateとする。
  - 2026-08-23: RC1を書いた64 GB実機SDでcompact seedから初回provisioningを実行。
    p2を8192 MiBへonline resize、p3を残り49.7 GiBで作成し、directory seedと
    `complete` markerまで同一sessionで完了。既存FAT32 signatureと4.5 GiBのcontentを
    非破壊再利用したため、blank p3のformat分岐は16 GB host fixtureの証拠を採用する。

## Boot artifact boundary

- [x] stock initramfsのIUX boot logoをplumOSへ置換する
  - 2026-08-14: `load_splash()`が`mount_flash()`より先に実行されるため、boot FATのOEM画像だけでは置換できないことを実機timestampで確定。stock Image/DTBを維持し、`post-flash.sh`がinitramfsの`ply-image`でマウント直後にplumOS画像を再描画する方式へ修正。IUXが一瞬表示された後に正しいplumOSロゴへ切り替わることを実機LCDで確認。初期IUXも完全に除去する場合だけstock Image内蔵initramfs画像の再packが必要。
- [x] stock SDのパーティション、kernel、DTB、initramfsを読み取り専用で解析する
- [x] stock userspaceを廃止し、保持するboot artifactの境界を決定する
- [x] SD先頭16 MiBのRockchip boot領域を管理者権限で読み取り採取する
- [x] ext4 `/storage` のfilesystem label、UUID、初回resize markerを確認する
  - 2026-08-23: RC1実機で`LABEL=PLUMOS_SYS`、固定UUID
    `504c554d-5354-4154-4500-000000000002`、8 GiB geometry、
    `/mnt/plumos/provision/ext4-resized`と`complete`をreadbackした。
- [x] boot artifactのprovenance、hash、サイズをmanifest化する
- [x] stock内蔵initramfsをboot substrateとして許容し、`SYSTEM`内init以降をplumOS所有にする方針を決定する
- [x] release imageをstock `Image`/stock由来のbounded DTB/stock kernel ABIへ戻す
- [x] stock initramfsの`SYSTEM` handoff contractをhostで再確認する
- [x] stock initramfsの`SYSTEM` handoff contractを実機で再確認する
- [x] stock initramfs hookとplumOS init早期logを実機SDから回収する
- [x] Linux 6.12 plumOS-owned kernel経路をexperimental扱いへ隔離する
  - 2026-08-22: build script、initramfs、専用testを`experiments/linux-6.12/`へ移動。
    `PLUMOS_ENABLE_EXPERIMENTAL_LINUX_6_12=1`の明示指定を必須とし、成果物も
    `output/experimental/linux-6.12/`へ隔離した。通常/System/SD/release buildから
    参照されないことを専用contract testで検証する。
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

- [x] RetroArchのCore Provided比率をgame/menu双方でPixel2 DRMへ正しく反映する
  - 2026-08-27: 通常coreのappend configによる4:3強制、90度scanoutとのcore geometry
    二重反転、DRM surface viewportによる補正前global aspect再参照の3点を修正。
    mGBA ModernのgameとRGUI menuはともに物理`427x640`、logical`640x427`となり、
    Runtime `0.1.0-dev-e4e689b`のhealthy/full checksumと実機目視に合格した。
    [検証記録](docs/validation/2026-08-27-pixel2-retroarch-core-aspect.md)を参照。
- [x] Pixel2の標準system pickerをV90S共通の3x2・6アイコンgridに揃える
  - 2026-08-14: 初期移植時の`default-horizontal` / `tile_strip`（2x1）を廃止し、標準`default` themeを`tile_grid`（3x2）、vertical page transitionへ変更。`c808952`の署名Runtime差分を実機適用し、3490 checksum、`runtime_healthy`、設定保持を確認。LCD上の6 tileと物理navigationは目視確認待ち。
- [x] 参照frontendをPixel2専用としてvendor化し、他機種・旧distribution名称を除去する
- [x] FE全機能を共有plumOS contractと照合し、欠落をrelease blockerにする
  - 2026-08-14: MFと同じ89 setting ID、START 7項目、Apps 12定義/7 visible、NW Service 5項目を機械監査。欠落AppsをP1扱いしていた監査と、Pixel2だけserviceを隠す分岐を廃止した。詳細は`docs/developer/frontend-feature-audit.md`。
  - 2026-08-15: [実機functional audit](docs/validation/2026-08-15-pixel2-frontend-functional-audit.md)で主要backendを再検証。Runtime `9da9bc7`、System `6a9fdfe`、FE/ADB、Runtime/System health、Network service stop後のcleanupに合格。物理menu acceptanceは継続。
- [x] START menuのReboot/Shutdown重複導線をPOWER 1項目へ統合する
  - 2026-08-23: 標準menu/feature contractを6項目へ変更し、`system:power`から
    Sleep/Reboot/Shutdown/Cancel共通menuを開くhandlerと、旧2項目が残らない
    catalog fixtureを追加。旧actionは既存設定との互換用に残す。
  - 2026-08-23: UGREEN AC650を挿したcold bootでRTL8821CUと`.107`の自動復帰を確認後、
    署名Runtime `0.1.0-dev-e3b44c5`（SHA-256 `f6ca135d...`）を適用。更新は
    `runtime_healthy`、Frontend componentとRuntime全checksumに合格した。実機上の正式FEを
    text-script経路でSTART -> 6番POWER -> 共通menuへ遷移させ、Sleep/Reboot/Shutdown/Cancel、
    Cancel初期選択、旧Reboot/Shutdown項目なしを確認した。
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
- [x] V90S準拠ADBのcold boot列挙、shell、物理抜き差しを実機再検証する（Wi-Fi優先・ADB廃止方針により対象外）
- [x] USB Wi-Fi上でSSH/SFTP/FTP/Sambaの認証・往復転送と再起動復元を実機検証する
  - 2026-08-17: `fd0fb34` RuntimeでADB/SSH/SFTP/FTP/Samba backendを実機確認。
    password認証、upload/download/delete、同一SHA-256、更新再起動後の5項目ON保持、
    component checksumは合格。さらに`192.168.10.147`のUSB Wi-Fi LAN越しで
    SSHログインとSFTP/FTP/Sambaのupload/download/deleteを反復し、全downloadの
    SHA-256一致と検証ファイル削除を確認した。
- [x] USB Wi-Fiのbulk-transfer性能調査を完了し、既知制約を確定する
  - 2026-08-17: `0bda:8179` / stock `r8188eu`で30 MiB SFTPが約59 KiB/sに停滞。
    storage、CPU、USB autosuspend、SFTP固有処理、省電力/IPS、HT/A-MPDU、
    `rtw_wifi_spec`を比較しても改善しなかった。別のWPA2-AES 2.4 GHz APまたは
    2台目の同USB ID dongleでAP相性と個体不良を分離する。詳細は
    [実機throughput調査](docs/validation/2026-08-17-pixel2-usb-wifi-throughput.md)。
  - 2026-08-17: 別driver/adapterの`0bda:c820` / `8821cu`を5 GHzへ接続すると
    SSH/FTP上りは2.1--3.2 MiB/sまで改善。一方SFTP上りは0.32--0.65 MiB/sで、
    SFTP traffic中だけTCP retrans/timeoutが顕著に増える。storage、client request数、
    cfg80211省電力を除外した。実用代替となるSSH/FTPとSFTP下りは2.1--3.69 MiB/sで、
    SFTP上りだけの追加改善策がないため0.32--0.65 MiB/sと再送を既知制約として受理する。

## Image and hardware validation

- [x] MBR、Rockchip boot領域、`PLUMOS_BOOT`、`PLUMOS_SYS`、`PLUMOS_USER`を生成する
- [x] image内のpartition境界、hash、SquashFS内容をhost検証する
- [x] 同一source refから生成したSD imageのSHA-256再現性をhost検証する
- [x] 通常起動のruntime integrity gateを高速化する
  - 2026-08-14: [実機boot profile](docs/validation/2026-08-14-pixel2-boot-profile.md)でkernel開始からFE renderer-readyまで約67.3秒を計測。1.2 GiB・3490 fileのroot checksumが52.60秒（約78%）、request無しPython updaterがcold時4.30秒、System slot checksumが1.16秒、ROM scanは174 msだった。
  - 2026-08-14: `25d1af5`で通常bootのfull hashとidle Python updaterを除外し、`46fb284`で明示`verify-runtime`をBusyBox対応。署名System A/B更新、slot readback、health昇格、active slot再起動に合格。最終通常bootはfrontend process開始8.28秒、renderer-readyは8.67秒以内、ADB 6.97秒、ROM scan 162 ms。完全3490 checksumは更新前と明示保守時だけ合格確認した。
  - 2026-08-22: full ROM scanだけ更新し、per-system cacheが残って旧拡張子や削除ROMを
    表示し続ける欠陥を修正。88 cacheをatomic renameし、再生成可能なcacheはbatchで
    directory durabilityを確定する。実機のcache writeは2.110秒から0.237秒、scan全体は
    6.397秒から4.515秒へ短縮し、通常bootへの追加負担を約1.88秒削減した。
- [ ] 複製SDでcold boot、LCD、input、audio、powerを実機検証する
  - 2026-08-23: `plumOS-Pixel2-0.1.0-rc1-28aaf65.img`（2,701,131,776 bytes、
    SHA-256 `ca9275c3...f108929`）を生成。release audit blocker 0、109/109 core、
    strict app-layer、System A/B、SD image verifier、host contract suiteに合格。
    この候補imageを書いた別SDでの物理最終確認を継続する。
  - 2026-08-23: RC1実機の初回boot/partition/readback、START -> POWER導線、
    Runtime全checksum、System A/B両slot SHA、通常reboot、FE復帰、保存SSIDによる
    UGREEN `0bda:c811`自動再接続に合格。代表RA/SA/PicoArch/PICO-8/Pyxelの
    process・DRM/fb0・ALSAを機械確認した。OpenBOR/Pyxelの画面はcapture不能、
    FE idleからkernel `mem` sleep、RTC復帰、表示/audio route再arm、Wi-Fi/FE/input
    service復帰にも合格。LCD/input/可聴音、実電源ボタンsleep、shutdown/chargingは
    このRC1 SDでoperator最終確認を継続する。
  - 2026-08-23: 上記SDはNEOGEO終了後にSD-backed機能が一斉停止し、次回電源ONで
    `NO SD`、再挿入後bootで`All phases bad` / tuning `-5`を記録した。generic `USD`、
    manufacturer/OEMとも0の媒体であり、再挿入後のRuntime/System A/B/ROM readbackは
    全合格。媒体または接点不良としてこのSDをrelease acceptanceから除外し、
    known-good branded SDで同じRC1の最終確認を継続する。
  - 2026-08-23: 別の`ASTC` SDでRetroArch/FBNeoの`aof.zip`を20回連続起動・終了。
    全回でemulator起動、DRM非黒画面、ALSA進行、RetroArch残留0、FE 1 process復帰、
    ROM SHA一致を確認した。周回前から存在したforced-off由来FAT dirty警告1件は増えず、
    MMC tuning/timeout/I/O errorは0、最終Runtime全checksumも合格。追加の判定器証明周回
    も合格し、Wi-Fiは10/10 ping、FE操作可能状態へ復帰した。この反復試験範囲では
    NEOGEO/FBNeoを前回の媒体脱落原因から除外し、generic `USD`媒体または接点不良という
    判断を維持する。詳細は`docs/validation/2026-08-23-pixel2-neogeo-loop.md`。
- [x] app-layer manifest/checksumを実機deploy単位で検証する
  - 2026-08-13: A/B slot A起動後、`checksums.sha256`の管理対象3450件が全て一致し、FEも`app-layer-verified`から起動した。
- [x] `/Volumes/public-1/02/motoki/emu/ROM/rom2`の代表ROMで全systemの実機起動・終了を検証する
  - 2026-08-12: PPSSPP統合後のhost route validationは代表ROMがある29 system中29 routeが`ok`、pending binaryは0。実機での全system起動・終了は未実施。
  - 2026-08-13: Pyxel統合後のhost route validationは代表ROMがある30 system中30 routeが`ok`、pending binaryは0。Pyxelを含む全systemの実機起動・終了は未実施。
  - 2026-08-13: Saturn廃止後は87 enabled、109 libretro core、standalone 4 built / 4 pending。ROM setのトップレベル、`_etc`、共有ATARI/MAME directoryを探索し、互換contentがある74 system・165 profileを抽出した。
  - [x] ROMセットに代表contentがある29 system・97 profileを実機で3秒起動し、97/97 early-start passを記録する
  - [x] archival/shared/directory-backed contentを追加検出し、現行routeで73 system・164 profileのearly-start passを記録する
  - [x] early-start passに使用した73 system・78代表セットを通常FEディレクトリへ復元し、目視確認へ引き継ぐ
    - 2026-08-14: 一時smoke contentの自動削除だけで終了していた手順を是正。p2=8 GiB、p3=残り49.7 GiBへ実機SDを拡張し、旧p3の651 fileを651/651 SHA一致で復元。pass report由来1,447 source file + 2 markerを恒久配置し、再適用で0 transfer / 1,447 identical skip、cold boot後73 system・83 FE entry、frontend readyを確認。
  - [x] Channel Fの必須BIOSをユーザーROMセットから正規に用意し、残る1 profileを起動確認する
    - 2026-08-22: Analogue Pocket用`cfbios.bin`の各1 KiB halfが`sl31253.bin`、
      `sl31254.bin`のupstream既知MD5と一致する場合だけ分割。FreeChaF説明で代替扱いの
      `sl90025.bin`はoptionalへ補正。実機でstartup、DRM image、ALSA pointer合格。
  - [x] ROMセットにmatching contentが無い13 enabled systemへ代表contentを用意し、実機起動を記録する（代表ROMなしの受理済み保留）
    - `ngp`, `wonderswancolor`, `x68000`, `tic80`, `vectrex`, `sg1000`, `sharpx1`, `wolf3d`, `zx81`, `arduboy`, `megaduck`, `puzzlescript`, `superbroswar`
  - [x] 2026-08-22時点の実機cacheにROMがある17 system・42 profileを、実emulator process、
    panel-size DRM image、ALSA playback pointerで再検査する
    - 38 profileは3項目合格。OpenBORは起動・音声合格だがDRM/fb0 captureが全黒のため
      実LCD表示を要目視。Channel F、ColecoVision、VMUは明確な起動失敗。
      [検証記録](docs/validation/2026-08-22-pixel2-device-media-smoke.md)を参照。
    - 静止画で明らかな回転・表示領域逸脱は見えない。可聴音、音質、音飛び、動的な
      ちらつき、厳密なaspect ratio、入力/menu/exit/saveはoperator確認を継続する。
    - 2026-08-22: BIOS復元後のtargeted recheckでChannel FとColecoVisionを3項目合格へ
      更新。VMUの108-byte `.VMI`はemulator contentではないdescriptorと判明し、catalogから
      除外した。現時点の有効content集計はmachine pass 40 route、OpenBOR manual display 1。
  - [x] 現在launchable ROMがある57 systemのうち未検査41 systemを同じ3項目で記録する
    - 2026-08-22: 全41 systemで実emulator processの継続を確認し、明確な起動失敗は0。
      自動総合は36 pass / 5 manual、screenは39 pass / 2 manual、audioは37 pass /
      4 manual。Lutroは非黒率0.8%で自動閾値未満だが、DRM captureでは正立したPong
      画面を確認した。PC-FXは白一色かつ音声継続なし。
      J2ME、TI-83、Uzeboxは画面合格だが試験場面でaudioを機械確定できない。
    - FDSは指定`Akumajou Dracula.zip`でRA FCEUmm/Nestopia、PicoArch
      FCEUmm/Nestopiaの4 profileをstartup/DRM/ALSA合格。30秒captureで悪魔城ドラキュラの
      title到達も確認した。内部名は`[b]`だが、今回の実起動確認には利用可能だった。
    - Neo Geo CD、ScummVM、Game & Watch、Lutro、PC-FXの誤content選択を修正。
      Game & Watchは初回`retro_run()`内の完全AV再初期化でPixel2 RetroArch DRM経路が
      `rc=139`となっていたため、初回はgeometryだけを更新するcore patchをビルドシステムへ
      必須化。署名Runtime `0.1.0-dev-f49d7ed`で15秒のstartup/DRM/ALSAを全て合格した。
  - [x] 現在launchable ROMが無い31 enabled systemへ代表contentを用意する（代表ROMなしの受理済み保留）
  - [x] ColecoVisionのBlueMSX BIOSをRA `system_directory`から読める形へmerge配置し、再起動する
    - 2026-08-22: BlueMSX `Machines`/`Databases`を含むBIOS復元後、実機でstartup、
      DRM image、ALSA pointerに合格。項目を完了扱いとする。
  - [x] VMUの`.VMI` descriptorをlaunchable contentから除外し、誤起動segfault (`rc=139`)を解消する
    - 2026-08-22: `vemulator`対応は`vms|dci|bin`で、ROMセットの`ANIMTEST.VMI`は108-byteの
      metadata descriptorだけだった。catalog、release audit、起動時per-system cache refreshを
      修正し、実機VMU ROM list 0件を確認。有効なVMS/DCI/BINがないためemulator自体の
      startup/display/audio acceptanceは未実施。
  - 2026-08-22: Runtime full checksum、A/B状態、partition、RTC/time sync、全物理key capability、
    RK817 audio route、network往復、temperature/battery、factory reset dry-runを追加監査。
    [検証記録](docs/validation/2026-08-22-pixel2-device-additional-audit.md)を参照。
- [x] fb0に残るstock/旧boot splash由来の残像をclearし、実機スクショ経路をplumOS化する
