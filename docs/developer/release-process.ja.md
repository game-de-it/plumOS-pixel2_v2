# リリース手順

[English](release-process.md)

これはplumOS Pixel2を公開する際の契約です。build成功はlocal candidateに過ぎません。
同じimageを別SDカードへ書き込み、実機Pixel2で検証して初めてrelease acceptanceとします。

## 事前条件

- release tagにするcommitで、全変更をcommitしたclean treeから作業します。
- 登録済みstock Pixel2 boot prefixとboot artifactを`artifacts/`に用意します。これは
  local build入力であり、source archiveには含めません。
- ROM、利用者BIOS、save、認証情報、proprietary PICO-8 runtime、mutable user stateが
  release入力に混入していないことを確認します。
- versionは`v`を付けないsemantic version、例`0.1.0`を指定します。予定tagは
  `v0.1.0`として記録されます。

## Buildとlocal gate

repository rootで完全準備コマンドを実行します。

```sh
./scripts/prepare-pixel2-release.sh --version 0.1.0
```

このコマンドは公開処理を行わず、次を実行します。

1. 独立libretro coreを4並列、共有Appも並列で、全required Pixel2 componentを再build
2. strict app layer、stock kernel SYSTEM、compact SD imageを生成
3. source契約、identity/content、implementation、license、app/System/image checksum、
   first-boot image testを実行
4. SD imageを同条件でもう一度生成し、SHA-256完全一致を要求
5. image圧縮、Git `HEAD`のsource archive、release notes、provenance、`SHA256SUMS`を生成
6. `.img.xz`を展開し、元imageとsize・SHA-256が一致することを検証

生成先は次です。

```text
dist/plumOS-Pixel2-v0.1.0/
  plumOS-Pixel2-v0.1.0.img.xz
  plumOS-Pixel2-v0.1.0-source.tar.gz
  RELEASE_NOTES.md
  RELEASE_MANIFEST.json
  SHA256SUMS
```

bundleだけを再検証する場合:

```sh
./scripts/verify-pixel2-release-bundle.py dist/plumOS-Pixel2-v0.1.0
```

## 実機release acceptance

release directory内の圧縮imageを展開し、別SDカードへ書き込みます。SHA-256を記録し、
最低限次を確認します。

- cold bootと初回setupの完走
- 最終`PLUMOS_SYS`・`PLUMOS_USER` partition構成と2回目boot後の保持
- FE操作、POWER menu、shutdown、reboot、sleep、resume
- USB Wi-Fi association、DHCP、抜き差し後の再接続、SSH、SFTP
- RetroArch、PicoArch、standaloneの代表game起動・menu・終了
- 画面方向・aspect、物理button、audio、volume
- OS起動中の充電とshutdown後のcharge mode

build出力、Raspberry Pi Imager完了、FEが1回表示されたことだけではacceptしません。
失敗は再build前に`TODO.md`と日付付き`docs/validation/`へ記録します。

## 公開境界

実機acceptance後だけ、`RELEASE_MANIFEST.json`の`source_ref`と同じcommitへ
`v0.1.0` tagを作り、release directory内の5ファイルを全てuploadします。tag後に
assetを再buildしません。

GitHub保存後はupload要求の成功表示を信用せず、再downloadして検証します。

```sh
./scripts/verify-pixel2-release-bundle.py \
  dist/plumOS-Pixel2-v0.1.0 \
  --download-base \
  https://github.com/OWNER/REPOSITORY/releases/download/v0.1.0/
```

この再download検証が成功してGitHub release完了です。
