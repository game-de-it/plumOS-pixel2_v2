# Pixel2 Moonlight New 接続・表示検証

日付: 2026-08-30  
対象: PortMaster adapter 53 / plumOS Runtime `0.1.4-dev-aecb3ec`

## 問題と原因

ペアリング済みhostへの接続時にGUIがFEへ戻る現象は、Moonlightバイナリのクラッシュでは
なかった。GUIが非同期の`moonlight list`完了前に`apps.txt`を再読込し、仮項目
`Load apps first`をstream対象としていた。

接続後のDesktopで赤と青が入れ替わる現象は、stock Pixel2 KMS/Mesa上でMoonlight
Embedded 2.7.0を共通SDL GLES2 rendererへ通した場合にだけ再現した。同じscanoutを
RGBとして解釈すると正常色になるため、stream出力のR/B swapと判定した。

## 実装

- commit `fc2e986`: `moonlight list`だけを同期実行し、仮項目を除外した。Pair処理は
  従来通り非同期のままにした。
- Pixel2 GUIのfont下限を28pxへ拡大した。
- 空の`ESUDO`を実行しないpass-through shimを追加した。
- 既存GUIはhash付きbackup後に原子的にpatchし、既知のupstreamだけを変更する。
- commit `aecb3ec`: `Moonlight New.sh`に限り`SDL_RENDER_DRIVER=software`を設定した。
  他のPortMasterアプリは既定のGLES2 rendererを維持する。

## 検証結果

host側ではMoonlight GUI fixture、PortMaster runtime test、app-layer script回帰、strict
app-layer build、license auditに合格した。実機へのmanaged deltaは一時fileへ転送し、
device側SHA-256確認後に原子的に置換した。全11332 managed fileのchecksumに合格し、
ユーザーのkey、設定、PortMaster更新物は保持した。

実機の通常FE経路で次を確認した。

- GUI正立、28px下限の文字、`Steam Big Picture`と`Desktop`の一覧表示
- 物理Aによるhost・Desktop選択とH.264 stream開始
- 640x480論理画面の正立表示と正常な色（operator確認を含む）
- ALSA streamが`RUNNING`になりhardware pointerが進行
- 管理停止後にMoonlight/launcher残留なし、FEは1processだけ復帰
- session mount 0、一時session directory 0、`session-cleanup result=clean`

取得した論理画面のSHA-256:

```text
5f2892e1dde35fb3232f2e9d9011a19d8cc9eae2c9f1c23f3c4986449a8c62e0  moonlight-gui-logical.png
5af9384615b4416052ad98bd800b59f1855508d5c767e3dd82b2bb271ccfc11f  moonlight-stream-logical.png
```

adapter 52のGLES2比較captureは
`5807e65fea741240e6ff51c0c9a54fc9b0b6c8f4ad950bfe584bdbd4688d2f85`で、
R/B逆転の比較証拠としてのみ保持する。
