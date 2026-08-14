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

検証時点でMacからADB deviceと外付けSDのどちらも見えていないため、packageは実機へ
未適用である。次はSystem、Runtimeの順で適用し、以下を実機確認する。

- cold bootでADBが自動列挙され、shellへ接続できる。
- NW Serviceに5項目、Appsに7項目、STARTに7項目が表示される。
- ADB明示OFF/ONと`plumos-enable-adb` recoveryが次回bootに反映される。
- USB Wi-Fi経由でSSH/FTP/SFTP/Sambaへ接続できる。
- 7 AppsとPortsを起動・終了し、FEへ正常復帰する。
