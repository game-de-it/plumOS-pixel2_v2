# Pixel2 RetroArch menu選択検証

日付: 2026-08-28  
実装source: `6581c54`  
実機Runtime: `0.1.2-dev-6581c54`

## 問題と原因

contentなしのRetroArch Appは利用者の`menu_driver`を使用していたが、game launcherは
software coreを常にplain DRMへ固定していた。graphical menuはGLESを必要とするため、
Ozone、XMB、GLUIを保存してもgame中には一貫して利用できなかった。

Ozoneは無回転viewport要求の直後にtexture cursor用の独自行列を複製していた。文字は後段の
回転済み行列を使う一方、cursorだけportrait座標に残り、landscape menu右側へ縦長の水色枠が
表示されていた。

## 修正

game launcherがmerge後のmutable `retroarch.cfg`を読み、RGUIは軽量なplain DRMを維持し、
GLUI、Ozone、XMBを明示選択した場合は`PLUMOS_GL_MENU_ROTATION=display`付きGLESを使用する。
`menu_driver`と`user_language`はappendしないため、利用者の選択が正となる。

Pixel2 GL backendはOzone、XMB、MaterialUIがcursor/icon用独自行列を複製する前に、固定menu
display回転を反映する。

## 検証

- menu選択fixture: RGUI、Ozone、XMB、GLUI、hardware GL RGUI、hardware GL OzoneでPASS。
- localization/menu contractと全app-layer script suite: PASS。
- frontendとRetroArchの正式並列build: PASS。
- strict app-layer検証とlicense audit: PASS。
- 署名Runtime SHA-256:
  `ab4ac4e4670d985302908602b569320fdaf7f0352afaee161204dc59ed1cb44e`。
- update結果: `runtime_healthy`。cold reboot後もsource `6581c54`で、利用者のOzone cfg SHA-256
  `1e9027253eac43c24203b5987dbab587decf641eb3fa403e88de9b569dc52960`は不変。
- 正式Ozone scanoutは640x480正立、選択cursorは横長で正常。通常QuickNES起動は
  `video_driver=gl`、rotation 1となり、非黒・正立640x480のgame scanoutを確認。

証跡:

- `output/live/2026-08-28-pixel2-retroarch-menus/formal/ra-formal-ozone-logical.png`
- `output/live/2026-08-28-pixel2-retroarch-menus/formal/ra-formal-game-logical.png`
- `output/live/2026-08-28-pixel2-retroarch-menus/ra-xmb-matrix-fix-logical.png`
- `output/live/2026-08-28-pixel2-retroarch-menus/ra-glui-matrix-fix-logical.png`

RetroArch内蔵screenshotはcontentなしAppではgraphical menu合成前の空core framebufferを取得し、
真っ暗になる場合がある。LCDへ送られた実画像はDRM scanout captureで検証する。
