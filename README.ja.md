# plumOS Pixel2

GKD Pixel2（RK3326S）向けのplumOSを再現可能にビルドするための
リポジトリです。

stock SDから再利用するのは、実機を起動するために必要なRockchip
ブート領域、vendor kernel、Pixel2 DTB、およびkernel ABIに一致する
module/firmwareだけです。stockのSquashFS、init、systemd unit、frontend、
設定、サービス、テーマ、名称はplumOS runtimeへ持ち込みません。

## 現在の段階

- stock SDのブート経路とハードウェアDTBを解析済み
- ブート必須artifactの採取契約を整備中
- plumOS所有のrootfs、SquashFS、SD image builderを実装中
- 初期bring-upではUSB ADBを既定の保守経路とし、USB Wi-Fiは任意機能とする

進捗と実機gateは `TODO.md` と `docs/` を参照してください。

