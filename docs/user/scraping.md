# Download thumbnail images

[日本語](scraping.ja.md)

Scraping downloads artwork for ROMs already visible in the frontend. A
supported USB Wi-Fi adapter and a working Internet connection are required.

## Run scraping

1. Connect Wi-Fi and confirm an IP address under Network Information.
2. Open `START -> Apps -> Scraping`.
3. Choose the image type: Box Art, Title Screen, or Screenshot.
4. Choose whether existing images are skipped or replaced.
5. Choose one system or all supported systems.
6. Start the operation and wait for the result screen.

Downloaded images are stored under `PLUMOS_USER/Images`. The frontend refreshes
its artwork view when the operation completes. The `Thumbnail Plan` app can
check matches without doing a full download, and `Thumbnail Results` reopens
the last result summary.

## Match quality

The scraper matches ROM names and available metadata. Region tags, revision
suffixes, translated names, hacks, and unusual archive names may not match.
Rename only your own ROM file when necessary; do not rename files inside arcade
ROM-set archives.

A `no match` result is not a network failure. A `download failed` result means
the network or source should be checked before retrying.
