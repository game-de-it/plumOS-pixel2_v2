# plumOS Pixel2

GKD Pixel2（RK3326S）向けのplumOSを再現可能にビルドするための
リポジトリです。

stock SDから採取するvendor kernel、Pixel2 DTB、module、firmwareは、初期解析と
ABI比較のための過渡的artifactです。最終imageではplumOS initramfsを組み込んだ
Pixel2対応kernelをbuildします。stockのSquashFS、init、systemd unit、frontend、
設定、サービス、テーマ、名称はplumOS runtimeへ持ち込みません。

## 現在の段階

- stock SDのブート経路とハードウェアDTBを解析済み
- ブート必須artifactの採取契約を整備中
- plumOS所有のrootfs、SquashFS、SD image builderを実装中
- 初期bring-upではUSB ADBを既定の保守経路とし、USB Wi-Fiは任意機能とする

進捗と実機gateは `TODO.md` と `docs/` を参照してください。

## System rootfs

arm64 Docker toolchainを使ってplumOS所有のSquashFSを生成する。

```sh
./scripts/build-tools-image.sh
./scripts/build-system-rootfs.sh
```

成果物は`output/system-rootfs/pixel2/payload/SYSTEM`に生成され、build中に
再展開、主要binaryの実行、manifest、旧distribution名の不在を検証する。

## Kernel

Linux 6.12.79、Pixel2/PX30S hardware support、plumOS initramfsから
`Image`、DTB、moduleを生成する。

```sh
./scripts/build-kernel.sh
```

hardware supportはcommit固定し、gamepadは標準`gpio-keys`、DSI compatibleは
`plumos,generic-dsi`としてbuildする。第三者codeの出所とlicenseは
`output/kernel/pixel2/source.manifest`と`licenses/`へ記録する。
