# Pixel2 additional device audit (2026-08-22)

## Scope

`2026-08-22-pixel2-device-media-smoke.md`の起動・DRM・ALSA検査に続き、
Wi-Fi接続中の実機で、ユーザーデータを変更しない診断と1 MiBの一時転送を行った。
転送用ファイルはSHA-256一致を確認した後、実機とMacの双方から削除した。

- device: `root@192.168.10.110`
- System: `0.1.0-dev-4993d8c`, active/booted slot A
- Runtime: `0.1.0-dev-16150d5`, transaction `runtime_healthy`
- battery: 91%（完全checksum後）; 追加NES再検査後89%

同日の修正完了時点ではSystem `0.1.0-dev-51e5ebd`（active/booted B）、Runtime
`0.1.0-dev-947df02`（`runtime_healthy`）へ更新した。最終完全Runtime checksum、
frontend component 199/199、FE起動、PID 1 BusyBox init、zombie 0に合格した。

## Passed checks

| area | result |
|---|---|
| Runtime integrity | 明示的なfull `verify-runtime`が1分19.51秒で合格 |
| System A/B | active/bootedともA、pending/attemptedなし、inactive B保持 |
| partition | p1=512 MiB、p2=8 GiB、p3=49.7 GiB、p3空き45.4 GiB |
| time | automatic time ON、同期済み、RK817 RTCとsystem timeの差0秒 |
| input capability | Power、Volume±、D-pad、ABXY、Start/Select、L/R/L2/R2、Function=704をkernel capabilityで確認 |
| audio route | RK817 PCM、Pixel2 state-only volume backend、speaker boost上限+15 dBを確認 |
| network services | SSH、SFTP、FTP、Sambaのenabled/runningを確認 |
| transfer | SFTP、FTP、Samba `SDCARD`で各1 MiBを往復し、全SHA-256一致 |
| updater | System/Runtimeにpending transactionなし、factory reset dry-run成功 |
| hardware | battery health Good、SoC約39.6 C、RAM約975 MiB、OOM/I/O errorなし |
| release audit | 88/97 systems enabled、required component 11/11、release blocker 0 |

SambaはmacOSの`smbutil view`によるshare列挙だけ`Broken pipe`となったが、
`mount_smbfs`で`SDCARD`を直接mountし、同じ往復転送に合格した。実用経路は動作するが、
macOS browse互換性は別途確認対象とする。

`plumos-storage-health`はRW mount中のFAT32に対して
`result=mounted-rw repair_required=unknown dirty_bit=true clean_shutdown_bit=false`
を返した。これはmount中にclean bitが落ちる既知の判定保留であり、破損判定ではない。

## Findings requiring follow-up

### Fresh SD BIOS loss and recovery

ホストのPixel2 BIOS manifest
`output/bios-staging/pixel2/plumos-bios-checksums.sha256`にある486 fileを、
実機`/mnt/plumos-user/bios`へread-only照合した結果は`0 OK / 486 missing`だった。
以前merge-only配置に合格した記録はあるが、現在のfresh SDへ引き継がれていない。
Channel FとColecoVisionの起動失敗はこの状態と整合する。ROM、BIOS、save、設定への
書込みはこの監査では行っていない。

その後、電池残量が十分ある状態で486件を競合0のmerge-only配置とし、実機で
486/486 SHA一致を確認した。ROMセットの別階層にはAnalogue Pocket用の
`cfbios.bin`があり、前半1 KiBが`sl31253.bin`、後半1 KiBが`sl31254.bin`の既知MD5と
完全一致した。BIOS inventoryは、この厳密な2つのMD5が一致する場合だけcompound fileを
分割するよう修正した。FreeChaF自身の説明に従い、代替`sl90025.bin`はoptionalとして
扱う。再生成結果は488 file、missing required 2、missing optional 35。実機も488/488
SHA一致となり、Channel FとColecoVisionはstartup、DRM image、ALSA pointerに合格した。

### PID 1 stopped reaping after the first frontend exit

現行Systemはboot setup後、最初のfrontend終了を待ってPID 1を`/bin/sh -l`へ
置き換えていた。このshellは孤児化したlauncher/emulatorを回収しないため、42 route
検査後にPPID 1のzombieが186件残った。profile別の正規stop helperを使うようdevice
smoke validatorを修正し、NES再検査ではRetroArchやlauncher shellの追加zombieを
防止できた。ただし現行PID 1ではfrontend切替由来の3件が追加された。

System側はone-shot boot setup後にBusyBox initをPID 1としてexecし、明示
`/etc/inittab`でrecovery consoleを保持する設計へ変更した。隔離コンテナでは意図的に
孤児化したchildを回収し、System A/B rootfs build/verifyにも合格した。実機の既存186件は
次回rebootで消える。修正版System `0.1.0-dev-51e5ebd`を現行`4993d8c`からのexact-source
署名packageとして通常A/B更新し、inactive Bへのreadback SHA一致、B boot/active、
`system_healthy`を確認した。PID 1は`/bin/busybox init`となりzombieは0。NES、Channel F、
ColecoVisionのFE→game→FEを含む再検査後もzombie増分0に合格した。

### Minor media hygiene

BOOTのsystem slot周辺に5件、user update inboxに2件のAppleDouble `._*`がある。
管理payload本体とchecksumは正常で、監査中は削除していない。既存のbounded
`plumos-sdcard-cleanup`対象として扱い、広範囲な手動削除は行わない。

### VMU descriptor and stale per-system cache

`ANIMTEST.VMI`は108-byteのmetadata descriptorで、`vemulator`の対応content
`vms|dci|bin`には含まれない。これをROMとして起動していたことが`rc=139`の直接原因で、
有効なpaired VMS dataはROMセット内に存在しなかった。Pixel2 catalogから`vmi`を除外し、
release auditへ非起動descriptor gateを追加した。

修正後も起動時full scanが`library-index.json`だけを書き換え、FEが読む
`state/frontend/systems/*.json`を残すため、VMUの古いentryが残った。full scanを88個の
有効system cacheの正本更新境界とし、各fileをatomic renameした後にdirectoryを1回だけ
同期する方式へ修正した。実機ではVMU cacheがROM 0件となり、`plumos-text-ui`でも空一覧を
確認した。cache writeは2.110秒から0.237秒、scan全体は6.397秒から4.515秒へ短縮した。

有効なVMS/DCI/BIN contentがないため、VMU emulatorそのもののstartup、画面、音は
未検証であり、誤ったVMI起動の失敗とは分けて扱う。

### Remaining launchable systems and Game & Watch

既存17 systemの検査後にROM cacheを持つ未検査41 systemをdefault profileで実行した。
全41 systemで実emulator processが継続し、明確な起動失敗は0となった。最終の自動集計は
次の通り。

| dimension | pass | manual | fail |
|---|---:|---:|---:|
| startup | 41 | 0 | 0 |
| panel-size DRM screen | 38 | 3 | 0 |
| ALSA pointer | 37 | 4 | 0 |
| overall | 35 | 6 | 0 |

初回検査の明確な失敗5件は、emulatorへ渡すcontent契約を絞って4件を解消した。
Neo Geo CDはMP3 trackのCUEを除外しWAV CUEを選択、ScummVMは`.scummvm`/`.svm`
markerを持つdirectory、Game & Watchは単一MGWを含むZIP、Lutroは`main.lua`を持つ
directory、PC-FXはCUE/CCD/TOC/CHDだけをlaunchableとした。

Game & Watchだけはcontent修正後も`gw_libretro`が`rc=139`となった。core dumpと一時診断
coreで、最初の`retro_run()`から`RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO`を発行し、Pixel2
RetroArchのvideo/audio/input driverを入れ子で完全再初期化する境界を原因と特定した。
初期`retro_get_system_av_info()`ですでに60 Hz / 44100 Hz timingは渡されているため、
game依存geometryだけを`RETRO_ENVIRONMENT_SET_GEOMETRY`で更新する必須patchへ変更した。
core patchはビルド時のdry-run/application gateとcore cache fingerprintへ含めた。

正式AArch64 core SHA-256は
`456400dcf5158446f085a89e40b02018847c3bdc7cb8e4501cefb050387a2ac6`。
exact-source署名Runtime `0.1.0-dev-f49d7ed`を`3ec83c2`から適用し、app manifestと
109-core manifestのsource ref、managed checksum、FE復帰、zombie 0を確認した。
Snoopyを15秒実行してstartup、正立DRM image、ALSA pointerの全項目に合格した。

manual 6件のうちLutroは非黒率0.8%のため自動閾値を下回っただけで、保存captureには
正立したPong画面がある。FDSはALSA進行中もcaptureが全黒、PC-FXは白一色でALSAが
PREPAREDへ戻る。J2ME、TI-83、Uzeboxは画面合格だが試験場面で音声を確定できない。
これらは起動失敗とは分け、実LCD・可聴音または別contentでの確認を継続する。

## Remaining acceptance boundary

- FDSの全黒画面とPC-FXの白一色画面／音声停止を別contentまたは実LCDで確認
- J2ME、TI-83、Uzeboxの可聴音（試験場面ではALSA進行を確定できない）
- launchable ROMがない31 enabled systemへの代表content準備
- OpenBORの実LCD表示
- 全routeの実音、操作、menu/exit、save/load、動的ちらつき
- inventoryで現在もmissing requiredの`ecwolf.pk3`と`kick34005.CDTV`
- VMUの有効なVMS/DCI/BIN contentを入手した後のemulator実機確認
- SambaのmacOS share browse列挙

## Host verification

```text
./tests/test-system-rootfs-scripts.sh
  system_rootfs_scripts=result-ok

./scripts/docker-build.sh system-rootfs
  system_dispatcher=result-ok
  system_rootfs=result-ok (slot A and slot B)

./scripts/docker-build.sh audit --release-gate
  implementation_audit=result-ok release_blockers=0

NES graceful-stop recheck
  startup=pass screen=pass audio=pass
  zombies=186 -> 189
  added: old frontend, text-ui, sdcard cleanup only
  not added: retroarch, launcher shell

Signed System device update
  source=0.1.0-dev-4993d8c target=0.1.0-dev-51e5ebd
  active=b booted=b result=system_healthy
  PID 1=/bin/busybox init
  NES round trip zombies=0 -> 0

BIOS recovery
  initial=0/486
  first merge=486/486
  Channel F compound split inventory=488/488
  Channel F and ColecoVision startup/screen/audio=pass
  post-test zombies=0

Final Runtime/cache refresh
  source=0.1.0-dev-05898c4 target=0.1.0-dev-947df02
  transaction=runtime_healthy full_runtime_verify=result-ok
  frontend checksum=199/199
  per-system caches=88 VMU roms=0
  scan total=6.397s -> 4.515s cache write=2.110s -> 0.237s
  NES startup/screen/audio=pass zombies=0

Remaining 41-system media smoke and Game & Watch fix
  startup=41 pass / 0 manual / 0 fail
  screen=38 pass / 3 manual / 0 fail
  audio=37 pass / 4 manual / 0 fail
  overall=35 pass / 6 manual / 0 fail
  Game & Watch runtime=0.1.0-dev-f49d7ed startup/screen/audio=pass
  frontend=running zombies=0 battery=55%
```

Evidence is retained under ignored local paths:

- `output/validation/pixel2-device-additional-2026-08-22/`
- `output/validation/pixel2-device-media-2026-08-22-reaper-check/`
- `output/validation/pixel2-device-media-2026-08-22-pid1-live-check/`
- `output/validation/pixel2-device-media-2026-08-22-bios-recheck/`
- `output/validation/pixel2-device-media-2026-08-22-channelf-fixed/`
- `output/validation/pixel2-device-media-2026-08-22-runtime-947df02-batch1/`
- `output/validation/pixel2-device-media-2026-08-22-runtime-947df02-batch2/`
- `output/validation/pixel2-device-media-2026-08-22-runtime-947df02-batch3b/`
- `output/validation/pixel2-device-media-2026-08-22-runtime-947df02-batch4/`
- `output/validation/pixel2-device-media-2026-08-22-runtime-947df02-batch5/`
- `output/validation/pixel2-device-media-2026-08-22-content-contracts/`
- `output/validation/pixel2-device-media-2026-08-22-gw-f49d7ed/`
- `output/validation/pixel2-device-media-2026-08-22-long-recheck-f49d7ed/`
- `output/validation/pixel2-device-media-2026-08-22-batch3-recheck-f49d7ed/`
