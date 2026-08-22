# 0002: plumOSがkernelとinitramfsを所有する（廃止済み実験）

日付: 2026-08-11
Status: Superseded by 0004

## 決定

この決定は0004により置き換えられた。Pixel2ではstockOSの
kernel/initramfs/reboot/charger経路をboot substrateとして利用し、
stock initramfsが`SYSTEM`へhandoffした後の`/sbin/init`以降をplumOSが所有する。

以下は、完全なfirst-userspace所有を目指していた時点の履歴として残す。
実装は`experiments/linux-6.12/`へ隔離され、明示的な環境変数なしには実行できず、
通常build、System/Runtime update、SD image、release imageの入力にはならない。

最終的なPixel2 imageでは、stock SDの`Image`をそのまま配布しない。
plumOSのinitramfsを組み込んだPixel2対応kernelを再現可能にbuildし、PID 1
以降をplumOSが所有する。

stock kernel 5.10.198、stock DTB、module、firmwareは初期解析とABI比較に
限定したvendor artifactとする。これらを使う過渡的imageはrelease不可とし、
明示的にbring-up専用と表示する。

## 根拠

stockの`Image`にはgzip圧縮されたinitramfsが内蔵されている。そのinitramfsは
boot FATの`SYSTEM`をloop mountし、stock Systemのinitへ切り替える。したがって
`SYSTEM`だけをplumOS SquashFSへ交換しても、stock userspaceを完全に廃止した
ことにはならない。

公開実装の調査では、次のPixel2 hardware enablementが確認できた。

- Linux 6.12.79を基準にしたRK3326 build設定
- `rk3326s-gkd-pixel2.dts`
- PX30S固有のthermal、GPU power domain、DSI PHY、I/O domain、USB PHY、
  suspend、OTP、DDRPHY、clock restore等のpatch
- mainline U-Boot v2025.10とRK3326/PX30S判定によるPixel2 DTB選択
- kernel config上のDWC2 dual-role、configfs、FunctionFS、およびUSB WLAN

調査snapshotは公開repositoryのcommit
`a4d24cf9d81bc05840773b5027751edb11c45c8b`である。これはhardware
enablementの技術的な参照元であり、そのdistributionのrootfs、service、設定、
theme、製品名をplumOSへ取り込むものではない。

## brandingとprovenance

実行時のOS名、hostname、USB gadget名、service名、path、manifest、boot menuは
すべてplumOSとして定義する。第三者由来コードを採用する場合、ライセンス上
必要なcopyright、SPDX、NOTICE、source URL、commit、変更内容は保持する。
法的帰属は製品brandingとは分離し、削除しない。

## 当時の実装gate（releaseでは使用しない）

release image生成には以下を必須とする。

1. pinned upstream kernel sourceとhash
2. repository内で管理したPixel2 patch/DTSとlicense provenance
3. plumOS initramfsを指定したkernel build
4. boot後の`/proc/1/exe`がplumOS initである実機証明
5. 成果物userspaceに旧distributionの名称、unit、設定がないこと

現行release gateはDecision 0004のstock 5.10.198 artifact境界を検証する。
