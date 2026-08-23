# サムネイル画像の取得

[English](scraping.md)

Scrapingは、フロントエンドに表示済みのROM用画像を取得します。対応USB Wi-Fi
アダプターとインターネット接続が必要です。

## Scrapingを実行する

1. Wi-Fiへ接続し、Network InformationでIPアドレスを確認します。
2. `START -> Apps -> Scraping`を開きます。
3. Box Art、Title Screen、Screenshotから画像種類を選びます。
4. 既存画像をSkipするかReplaceするか選びます。
5. 対象システムまたは全対応システムを選びます。
6. 開始して結果画面が表示されるまで待ちます。

取得画像は`PLUMOS_USER/Images`へ保存され、完了後にフロントエンドの画像表示が
更新されます。`Thumbnail Plan`は大量取得前のmatch確認、`Thumbnail Results`は
前回結果の再表示に利用できます。

## 名前の一致

ScraperはROM名と利用可能なmetadataを照合します。region、revision、翻訳名、hack、
特殊なarchive名は一致しない場合があります。必要な場合は自分が所有するROMファイル名だけを
変更し、arcade ROM setのzip内部はrenameしないでください。

`no match`はnetwork failureではありません。`download failed`の場合は、ネットワークや
取得元の状態を確認してから再実行してください。
