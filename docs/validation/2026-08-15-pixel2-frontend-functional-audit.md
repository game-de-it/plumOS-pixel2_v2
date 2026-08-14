# Pixel2 frontend functional audit

## Scope

共有plumOS frontend contractのSTART、Apps、Network Service、主要System Settingsを、
生成payloadと起動中Pixel2の双方で再監査した。ADBからbackendを直接実行する試験と、
LCD上で物理buttonを操作するacceptanceは区別する。

## Fixed defects

- RetroArch Appはゲームlauncherと異なり`plumos-ensure-udev-input-db`を呼んでおらず、
  `udev`がPixel2 padを発見できずに`Cannot initialize input driver`で終了していた。
  contentless RGUIにも同じudev contractを適用した。
- PortMasterが参照する`plumos-frontend-stop`と`plumos-frontend-launch`がpayloadに無かった。
  Pixel2のSystem initへ接続するhelperを追加した。
- 終了済みfrontendとhardware-key daemonがzombieになった場合、`kill -0`だけの判定が
  runningと誤認した。`/proc/PID/stat`のstateとcmdlineを検証するようにした。
- reboot/shutdown前処理を同じfrontend stop helperへ統一し、zombie PIDを3秒待ってから
  KILLする経路を除去した。
- FE System UpdateはFAT mtimeだけでwildcard-source packageを自動選択でき、旧開発版へ
  downgradeする余地があった。自動選択は現在版をsourceとして明記したupdate chainだけを
  許可し、wildcardは明示指定のrecovery専用にした。
- update候補scanが保管中の全tar.gz payloadを読み、旧package約900 MiBがある実機で
  30秒を超えていた。先頭の署名metadataで候補を絞り、選択した1 packageだけを従来通り
  archive member、SHA-256、payloadまで完全検証する二段階方式にした。

## Host validation

- `./tests/test-app-layer-scripts.sh`: pass
- `./tests/test-system-rootfs-scripts.sh`: pass
- `./tests/test-pixel2-update.sh`: pass
- `scripts/audit-pixel2-implementation.py`: release blockers 0
- app-layer assemblyと全component checksum: pass
- START 7、Apps 12定義/7 visible、Network Service 5、required setting 89を維持

## Device results

最終実機状態は次の通り。

```text
Runtime: 0.1.0-dev-9da9bc7, runtime_healthy
System:  0.1.0-dev-6a9fdfe, slot a/a, system_healthy
Frontend: running
ADB: running, enabled=1
```

| Surface | Result | Evidence / boundary |
| --- | --- | --- |
| System grid | pass | DRM captureで3x2、6 system tileを確認 |
| Scraping | pass | 代表ROMを対象にthumbnail planが完走。fetchは非実行 |
| File Manager | early-start pass | NextCommander processが6秒以上継続 |
| Music Player | early-start pass | `Clock.wav`配置済み。回転後logical bounds修正後6秒以上継続 |
| RetroArch App | pass | RGUIが8秒以上継続し、`pixel2_joypad` udev DB生成を確認 |
| Pyxel Setup | status pass | Pyxel 2.9.3、pygame 2.6.1、numpy 2.4.6、Pillow 12.3.0 import成功 |
| PortMaster | early-start pass | bootstrap processが6秒以上継続 |
| Update PortMaster | implementation pass | atomic staging、archive検証、foreign adapter除去をhost test。network installは非実行 |
| ADB | pass | USB接続、status summary、boot後自動復帰を確認 |
| SSH | service pass | 一時Ed25519 keyでDropbear起動、pidfile、stop後process消失を確認。鍵は削除済み |
| FTP | service pass | start/status/stopを確認 |
| SFTP | service pass | 一時key条件でstart/status/stopを確認 |
| Samba | conditional pass | Wi-Fi無しでは`waiting_network`、stopを確認 |
| Display / volume | pass | Pixel2 backlightとRK817 stateを取得。物理輝度/音量変更は既に合格済み |
| Time | pass | system/RTC offset 0秒 |
| Storage | bounded status pass | mounted-rwのためrepair判定保留。通常bootではfsckしない |
| Factory reset | dry-run pass | RA、PicoArch、SA、allの対象fileを列挙し、変更なし |
| System Update | pass | Runtime 2世代とSystem A/B 2世代を署名適用しhealth昇格 |
| Update no-candidate | pass | 旧wildcard packageを選ばず3秒で終了、request stateはclean |
| Reboot | pass | update適用を含むsafe rebootとFE/ADB復帰を複数回確認 |
| Shutdown | dry-run pass | actual poweroffはこの監査では行わず、既存RK817 acceptanceを保持 |

ROM set由来の73 system・78代表set、BIOS staging 488 file、Music sampleは
`/mnt/plumos-user`へmerge-only配置したまま保持している。

## Remaining physical acceptance

- FE画面からSTART各項目と7 Appsを物理buttonで選び、表示、入力、終了後FE復帰を確認する。
- Update PortMasterの実network更新と、USB Wi-Fi経由のSSH/FTP/SFTP/Samba接続を確認する。
- shutdownをFEの確認dialogから実行し、完全poweroffを確認する。
- emulator/coreごとの画面、全入力、音声、Function menu、save/stateは既存TODOを継続する。

