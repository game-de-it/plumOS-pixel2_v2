# Pixel2 frontend feature recovery validation

## Scope

起動時ADB、STARTメニュー、Apps、NW Service、Ports導線をMF/V90S系の共有frontend
contractと比較し、Pixel2で失われていた機能をrelease gate込みで復旧した。

## Root cause

欠落はPixel2 hardwareやstock boot substrateの制約ではなく、bring-up時の暫定実装と
監査不足によるものだった。

- `f5a3bfc`でfresh imageのADB既定値をOFFへ変更していた。
- FEは`/mnt/plumos/config/network/services.conf`へ保存する一方、SystemのADB serviceは
  `/state/plumos/config/network/services.conf`を読んでいた。
- Pixel2 controllerがFTP/SFTP/Sambaを意図的に隠し、backendも`not_installed`を返していた。
- Apps catalogはScrapingとPyxel Setupだけで、欠落AppsをP1扱いする監査ではreleaseを
  阻止できなかった。
- HELPはPixel2のDropbear port 22ではなくMF由来のport 2222を案内していた。
- PortMaster Appは存在したが、`ports` systemが`pixel2-portmaster-pending`として無効だった。

## Implemented contract

- 設定未作成時のADBをONへ戻し、明示した`adb_enabled=0/1`を次回bootへ保持する。
- FAT32 user volume rootの`plumos-enable-adb`を明示OFFからも使えるrecovery overrideとする。
- network service設定を`/mnt/plumos/config/network/services.conf`へ統一する。
- enabled serviceのboot再開を非同期・非致命的にし、任意serviceの失敗でFE起動を止めない。
- SSH、FTP、SFTP、Samba、ADBの5 serviceをcomponent化する。
- Scraping、File Manager、Music Player、RetroArch、Pyxel Setup、PortMaster、
  Update PortMasterの7 visible Appsをcomponent、launcher、checksum込みで収録する。
- `roms/ports/*.sh`をPixel2所有のPortMaster adapterへ接続する。
- 89 setting、START 7項目、Apps 12定義/7 visible、NW Service 5項目、11 mandatory
  componentsを機械可読contractとrelease gateで固定する。

比較結果は次の通り。

| Target | Required settings | START | Apps total/visible | NW services |
| --- | ---: | ---: | ---: | ---: |
| Pixel2 | 89/89 | 7 | 12/7 | 5 |
| MF | 89/89 | 7 | 12/7 | 5 |
| V90S | 83/89 | 7 | 12/7 | 5 |

V90Sとの差6 settingはbattery temperature、lid、audio outputなどhardware/version surface
の差であり、Pixel2はMFの共有89 settingを満たす。

## Host validation

以下を2026-08-14に実行し、すべて合格した。

- `./scripts/docker-build.sh audit --release-gate`
  - system: 88 enabled / 97 total
  - visible Apps: 7
  - mandatory components: 11/11
  - release blocker: 0
- `./tests/test-app-layer-scripts.sh`
- `./tests/test-system-rootfs-scripts.sh`
- `./tests/test-implementation-audit.sh`
- strict app-layer assembly and checksum validation
- AArch64 `ports/test.sh` fixture
  - `launch_profile: external:port`
  - `can_execute: yes`

SaturnはRK3326方針により明示的に無効のままにする。content policy 33件と未使用の
standalone代替binary 4件は監査/TODOに残し、未実装を非表示にはしていない。有効な
frontend routeには未解決launcherがない。

## Signed recovery packages

機能commit `67c25aa`を収録したfull packageを生成した。

| Apply order | Package | SHA-256 | Payload |
| ---: | --- | --- | ---: |
| 1 | `plumos-pixel2-system-0.1.0-dev-67c25aa.tar.gz` | `bf91019dbfa7ae3de8c309c6c17df550be4c3309f57f85902d7f88f424013baa` | 1 file |
| 2 | `plumos-pixel2-runtime-0.1.0-dev-67c25aa.tar.gz` | `6bb20bf6c27137def8751ed03589cbd9f4e0748b842779cd0df2f382a852de40` | 3721 files |

両packageは外側SHA-256、Ed25519 manifest署名、安全なarchive path、秘密鍵非混入を
検証した。System payload SHA-256
`2de1e85abcc23f5923d8680204317a3f42d272294f01a88d75ed0b9aa680191e`は生成したA/B
SquashFSの双方と一致する。Runtimeは既知のdevice baseを推測しないfull payloadである。

## Hardware acceptance pending

初回のhost検証時点ではMacからADB deviceと外付けSDのどちらも見えていなかった。
その後、同日中に物理SD `/dev/disk4`を接続し、System recoveryを実施した。

### Offline System recovery

- SD layout: 512 MiB `PLUMOS_BOOT`、8 GiB ext4 Runtime、残容量`PLUMOS_USER`
- installed generation before recovery: dispatcher/A/Bすべて`d56bf29`
- macOSのraw-device policyが管理者権限の`debugfs`も拒否したため、ext4へ通常updaterの
  `request.json`を登録する方法は使用できなかった。
- 旧dispatcher/A/B/metadataを
  `/updates/offline-system-backup-d56bf29-20260814-2148`へ退避し、既存
  `checksums.sha256`の7/7 entryに合格した。
- 新dispatcher/A/B/metadataを一時名でFAT32へ書き、readback SHA一致後に確定名へ
  切り替えた。最終boot payloadは新`checksums.sha256`の7/7 entryに合格した。
- stock `Image` SHA-256は
  `853eb041f1042a5f54ab66143cc8babb3942936f5c5209bc0c05d439ec3bd466`、DTBは
  `a7a438f705f994a9f333b2f334a803d47bc00cae6ed4556d51c730604452757a`のまま保持した。
- `/plumos-enable-adb` recovery markerとSystem/Runtime packageを`PLUMOS_USER`へ
  merge-only配置した。ROM、BIOS、設定、既存user dataは削除していない。

System A/Bは`0.1.0-dev-67c25aa`へ更新済みである。Runtimeは端末側のtransactional
updaterを使うため未適用であり、System boot後にADBから明示requestする。続いて以下を
実機確認する。

- cold bootでADBが自動列挙され、shellへ接続できる。
- NW Serviceに5項目、Appsに7項目、STARTに7項目が表示される。
- ADB明示OFF/ONと`plumos-enable-adb` recoveryが次回bootに反映される。
- USB Wi-Fi経由でSSH/FTP/SFTP/Sambaへ接続できる。
- 7 AppsとPortsを起動・終了し、FEへ正常復帰する。

## Provisioning incident and recovery

上記offline recovery記録後の最初の実機bootで、既存p3を保護するはずのprovisionerが
p3を3回再formatした。したがって、上記の「既存user dataは削除していない」はoffline
書込み操作そのものについては正しいが、その後のbootを含む最終結果としては正しくない。
実機にstageしたupdate packageと検証sentinelは失われた。format前にp3上のROM/BIOSを
完全に退避できていたことは証明できないため、user data lossの可能性を残す。

原因はSystemの`/bin/blkid`がBusyBoxへのsymlinkだった一方、採用BusyBoxに`blkid`
appletが無く、FAT32判定が毎回失敗したことだった。`b902e3e`で以下を修正した。

- util-linuxの実体`/usr/sbin/blkid`をSystemへ収録する。
- `blkid`が起動不能でもboot sectorのtype/labelを直接読んで既存FAT32を判定する。
- 既存p2/p3/stateがbyte-for-byte不変であるmissing-blkid fixtureを追加する。

修正版Systemをinactive Bへ直接stageしてbootし、実体`blkid`の成功、p3の
`TYPE=vfat` / `LABEL=PLUMOS_USER`、sentinel SHA保持、format countが3から増えないことを
確認した。その後の署名System `0.1.0-dev-304459a`も通常A/B updater経由でslot Aへ適用し、
`system_healthy`まで合格した。

## Complete Runtime update incident

最初のfull Runtime packageは`bin/ftpd`と`bin/tcpsvd`の親directory symlinkを含み、
端末updaterのpath safety gateに拒否された。`53268ad`で自己完結wrapperへ変更し、package
builderとapp-layer verifierの双方で不正symlinkを拒否するようにした。

さらにfull package inventoryの照合により、Pixel2 updaterのmanaged rootから
`fonts`、`network`、`ssh`、`config/frontend/feature-contract.json`が漏れていたことを検出
した。`304459a`でbuilder/updater双方のmanaged contractを揃え、4135 payload file、
uncompressed 1,459,145,397 byteの署名packageを生成した。

```text
plumos-pixel2-runtime-0.1.0-dev-304459a.tar.gz
sha256=21d8b1e0089e16b10633db58f97e0193c49c5c16a9d2699fc7b7bbf8c8219a5d
fonts_files=2
network_files=406
ssh_files=5
```

このpackageの初回実機適用で、V90S由来updaterが各fileごとに増大するtransaction JSON
全体を複数回atomic writeしていることが判明した。4135操作では12,405 journal write、
累計約2.96 GBとなり、各file/metadata fsyncも加わるためPixel2のSD上で長時間bootを
停止させる。加えてPixel2 Systemはupdaterが要求する6枚のprogress raw frameを生成・
収録しておらず、LCDはplumOS boot logoのままだった。

- `89fa6a4`: native 480x640 BGRA、論理640x480 CCWの6段階update frameを生成・収録し、
  欠落とsizeをSystem verifierでrelease-blockingにした。
- `164c841`: 完全な操作計画を変更前に1回だけjournalへ書き、backupの存在を開始済み操作
  のdurable proofとしてrollbackする線形I/O方式へ変更した。未着手の既存fileをrollback
  が削除しないinterrupted fixtureを追加した。

`0.1.0-dev-164c841` System packageはhost build/verificationに合格した。

```text
plumos-pixel2-system-0.1.0-dev-164c841.tar.gz
sha256=4d579f0d465acbd7ddea9fe100c0ae3519575865ff9d892b7ac6dd081db826cc
```

旧updaterで開始済みのRuntime transactionと、続く修正版Systemの物理acceptanceは継続中。

## Compact image physical first boot

`e1b05ed`から生成したcompact imageを64 GB実機SDへ書き、初回storage setup画面と
frontend起動を確認した。provision logでは14:44:25から14:44:26までの約1秒で以下を
完了していた。

- p1: start 32768、1048576 sector、512 MiB `PLUMOS_BOOT`
- p2: start 1081344、16777216 sector、8 GiB `PLUMOS_SYS`
- p3: start 17858560、104280064 sector、49.7 GiB `PLUMOS_USER`
- online ext4 resize: 524288 blockから2097152 block
- `state=complete`、`userdata-seeded`、`.plumos-ready`

短時間だった理由は、compact image終端より後ろに旧p3の有効なFAT32 signatureが残って
いたためである。preserve policyに従ってformatを省略し、既存`PLUMOS_USER`を再利用した。
このため`p3-formatted` markerとformat logが無いこと自体は異常ではない。

ただし同じ初回boot内で、provisionerが既にp3を`/mnt/plumos-user`へmountした後、initが
同じmountを再実行して失敗し、fallback tmpfsを上からmountしていた。p3の49.7 GiBは
隠れ、初回sessionで書くROM/BIOSが再起動時に失われる状態だった。通常再起動後は
provisionerのbounded complete pathがmountを行わないため、p3は1回だけmountされて正常化
した。

再起動後の実機acceptance結果:

```text
system=0.1.0-dev-e1b05ed
runtime=0.1.0-dev-e1b05ed
frontend_ready=yes
mount_count(/mnt/plumos-user)=1
tmpfs_overlay=no
PLUMOS_USER=52127264 KiB, used 2%
complete=yes
state=complete
user_ready=yes
format_count=0
complete_count=1
required_user_directories=10/10
boot_errors=0
```

`cf8e96f`で、provisioner後に`/mnt/plumos-user`が既にmount済みならinitがそのmountを採用し、
二重mount/fallbackを行わないguardを追加した。次のcompact imageで初回sessionからp3が直接
見えることを物理確認する。
