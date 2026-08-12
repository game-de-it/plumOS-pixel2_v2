# Pixel2 Implementation Inventory

最終更新: 2026-08-13
監査ベース: 2026-08-13 P0 backend実装後の生成app-layer

この文書は「buildできる」「FEに表示される」「hostでrouteが解決する」
「Pixel2実機で合格した」を区別する、実装作業のsource of truthである。
詳細な作業チェックボックスはルートの `TODO.md`、個々の証跡は
`docs/validation/` に記録する。

## Status definitions

| 状態 | 完了条件 |
| --- | --- |
| Implemented | pinned source、build target、runtime、FE導線、component manifest/checksum、host testが揃う |
| Host verified | clean sourceから生成し、target binary実行、app-layer、SD image、route検証が合格する |
| Device verified | 実機で表示、全入力、音声、終了、FE復帰、save/state、再起動後保持を確認する |
| Release ready | device verifiedに加えて更新、security、license、配布物、再取得checksum gateが合格する |
| Pending | 上記のどこが不足しているかをこの文書とTODOに明記し、release gateを通さない |

## Automated audit baseline

`./scripts/docker-build.sh audit` はfrontend catalogと生成app-layerを照合する。
`./scripts/docker-build.sh audit --release-gate` は、ユーザーに公開されているのに
実装がない項目が1件でもあれば失敗する。`release-image`にも同じgateを組み込んだ。

2026-08-13の監査結果は次の通り。

| 対象 | 結果 |
| --- | ---: |
| frontend systems | 97 total / 88 enabled / 9 disabled |
| required app-layer components | 7 / 7 present |
| libretro cores | 114 built |
| standalone emulator | 3 built / 7 pending |
| visible Apps entries | 2 |
| enabled systems with pending content policy | 33 |
| release blockers detected by audit | 2 |

## Implemented build surface

以下はbuild systemとapp-layerへ統合済みである。ただし、Host verifiedは
Device verifiedを意味しない。

- stock Rockchip prefix、stock kernel/DTB/initramfs substrate、plumOS `SYSTEM` handoff;
- plumOS init、ADB bring-up、USB Wi-Fi、Dropbear SSH、persistent logs;
- frontend、text UI、ROM scanner、START menu、Pixel2 input contract;
- RetroArch 1.22.2と114 libretro core catalog;
- PicoArchと共有libretro core route;
- OpenBOR、DraStic、PPSSPP standalone;
- Pyxel/Python 3.11/pygame/numpy/Pillow runtimeとPyxel Setup;
- RK817/USB向けALSA `plumos_output`、global volume/brightness service;
- strict app-layer metadata、SquashFS `SYSTEM`、4 GiB SD image generation。

## P0: exposed but incomplete user surface

以下はユーザーが現在のFEから到達できる、または選択肢として表示されるが、
実装が完結していない。全項目をrelease blockerとする。

| 項目 | 現状 | 必要な実装 |
| --- | --- | --- |
| System Update | signed Ed25519 package、Runtime journal/1世代rollback、inactive System readback、FE request/safe rebootをhost実装済み | 実機FEからRuntime/System成功、失敗rollback、表示/logを検証 |
| Storage Check | `/mnt/plumos-user`に対する同梱`fsck.fat -n`、45秒上限、status/logを実装。RW mount中は誤警告せず判定保留 | RO状態での実機clean/dirty検証 |
| Factory Reset | `factory-defaults/{ra,pico,sa}`とbackup/atomic restore/dry-runを実装 | 実機対象別restoreと再起動後確認 |
| Time Settings | bounded RFC868同期とRK817 RTC UTC保存を実装 | 実機RTC read/write、timezone/manual-time、再起動後保持 |
| PSX alternate SA | `standalone:pcsx_rearmed`を公開、binary pending | Pixel2 build/runtimeを実装・検証してから公開状態を合格にする |
| Saturn alternate SA | `standalone:yabasanshiro`を公開、binary pending | Pixel2 build/runtimeを実装・検証してから公開状態を合格にする |

Pixel2では存在しないAudio Output切替、Lid Suspend、FTP/SFTP/Sambaをdevice
capabilityにより表示しない。これは未実装項目を隠す処置ではなく、物理hardwareと
imageが所有しない機能を操作可能と誤表示しないための契約である。

ADB daemon自体はhost-key challengeを持たないため、release defaultをOFFにし、FEで
保存した`adb_enabled=1`またはuser FAT32 rootの`plumos-enable-adb` markerだけを
boot opt-inとして扱う。新SYSTEMでON/OFF/recovery markerをcold boot検証する。
SSHは`authorized_keys`の有無に加えてUI toggleとboot状態の一致を別途確認する。

## P1: application and standalone parity

shared plumOS handheld surfaceにあり、Pixel2で未実装のAppsは次の5項目である。
メニューから隠すことを実装完了の代替にはしない。各componentはpinned source、
Pixel2 launcher、managed dependencies、mutable state、manifest/checksum、license、
host test、実機acceptanceを揃えてから完了にする。

1. File Manager / NextCommander
2. Music Player
3. RetroArch menu
4. PortMaster
5. Update PortMaster

standalone manifestで`pending-binary`なのは次の7件である。

- PCSX-ReARMed;
- YabaSanshiro;
- ScummVM;
- EasyRPG;
- Flycast;
- Mupen64Plus;
- NXEngine-Evo。

ScummVM、EasyRPG、Flycast、Mupen64Plus、NXEngineにはlibretro default routeが
すでに存在する。standalone実装はdefault routeの動作確認とは別の追加作業である。

## P1: emulator and content validation

88 enabled systemsのうち33 systemはruntime/coreが存在する一方、arcade ROM set、
disk image、multi-file data layout、frontend policy、scraper sourceのいずれかが
未確定である。

| policy | systems |
| --- | --- |
| arcade ROM set | arcade, cps1, cps2, cps3, fbneo, neogeo |
| disk image | amiga, atari800, atarist, c64, cpc, pc88, pc98, sharpx1, thomson, vic20, x68000, zx81, zxspectrum |
| data layout | cannonball, cavestory, chailove, dinothawr, lowresnx, lutro, microw8, quake, wolf3d |
| frontend policy | j2me, music, ti83, vmu |
| scraper source | uzebox |

disabledの9 systemは、mame2003plus、ports、2048、bk、daphne、flashback、mrboom、
palm、rickdangerousである。`ports`はPortMaster実装に依存する。他はcontentless、
data layout、frontend policyを決めたうえで再評価する。

ROM set route testは代表ROMが存在する30 systemのhost routeを解決しているだけで、
全88 systemの実機動作保証ではない。各systemで以下を記録する。

- content/BIOS layoutと代表ROM hash;
- launch profile、core/emulator build revision;
- LCD orientation、aspect、frame pacing;
- D-pad、ABXY、START/SELECT、L/R/L2/R2、FUNCTION、exit hotkey;
- audio、volume key、save/state、clean FE return;
- second launchとreboot後の設定保持。

## P1: first boot, storage, and update foundation

現在の4 GiB imageはp1 512 MiB、p2 2048 MiB、p3 remainderを事前生成する。
first-bootで未実装なのは次の通り。

- first bootでp2を8192 MiBへ拡張し、残りにp3を非破壊作成するprovisioner;
- interrupted provisioningのresumeと既存p3保護;
- ext4 label/UUID/resize markerの実機確認。

System A/B selector、pending boot、renderer-ready promotionは実機成功経路まで、
Runtime write-ahead journal、1世代rollback、inactive System update、Ed25519
builder/verifier、公開鍵、compatibility gate、FE request、safe rebootはhost fixture
とARM64 chrootまで実装済みである。署名packageの実機成功/失敗経路をrelease前に
別途acceptanceする。

## P1: physical-device acceptance

以下はコードまたはhost artifactがあるが、最終構成で実機合格が残る。

- clean SD cold boot、LCD、全physical input、audio、volume、brightness;
- FE -> emulator/app -> FEでforeground ownerが常に1つであること;
- OpenBOR、DraStic、PPSSPP、Pyxelの表示、入力、音声、終了、state保持;
- representative ROM 30 systemと、ROMが準備できる残りsystem;
- FE menuからのactual shutdownと、充電中reboot;
- ADB再接続、USB Wi-Fi dongle、SSH public-key login;
- save/stateとactive settingsがupdate/deploy後も保持されること;
- app-layer root/component checksumを含むatomic live deployment。

## P2: release and repository readiness

GitHub公開前に、実装とは別に次を整える。

- English/Japanese READMEを実装変更時に同期するgate;
- top-level project license、third-party notice、closed DraStic redistribution条件の再確認;
- CIでhost tests、forbidden identity/content、audit、manifest/checksumを実行;
- versioned release artifact、SHA256SUMS、archive内容検査、再download checksum;
- stock capture、ROM、BIOS、credentials、saves、user stateをreleaseから除外;
- support matrixと既知の制約をuser documentationへ反映。

## Execution order

1. P0の公開済み未実装surfaceをすべて実装し、audit blockerを0にする。
2. Scraping、File Manager、Music Player、RetroArch menu、PortMasterをcomponent化する。
3. 7 standalone pending binaryを順にbuildし、default/alternate routeを実機検証する。
4. 33 content policyと9 disabled systemをROM setに基づき解決する。
5. first-boot provisioning、System A/B、signed updaterを実装する。
6. clean imageを複製SDへ書き、全physical-device acceptanceを完走する。
7. repository/release gateを通し、配布物を再取得して最終checksumを確認する。

`release-image`が成功することを、このリストの完了と同一視しない。最終完了は
監査blocker 0、TODO完了、dated validation、実機acceptanceの4条件で判定する。
