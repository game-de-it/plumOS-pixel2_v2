# TODO

## Boot artifact boundary

- [x] stock SDのパーティション、kernel、DTB、initramfsを読み取り専用で解析する
- [x] stock userspaceを廃止し、保持するboot artifactの境界を決定する
- [x] SD先頭16 MiBのRockchip boot領域を管理者権限で読み取り採取する
- [ ] ext4 `/storage` のfilesystem label、UUID、初回resize markerを確認する
- [x] boot artifactのprovenance、hash、サイズをmanifest化する
- [x] stock内蔵initramfsを最終imageで廃止するkernel所有方針を決定する
- [x] Pixel2対応kernel/DTS/patchをpinned sourceからplumOSとしてbuildする

## plumOS System

- [x] plumOS Pixel2 rootfsを再現可能に生成する
- [x] plumOS initramfsでboot FATとSystemをmountし、initでstateとROMをmountする
- [x] stock由来名称・unit・frontendがSystemへ混入しないgateを実装する
- [x] Pixel2 kernel moduleと最小USB Wi-Fi firmwareをSystemへ統合する

## Frontend

- [x] plumOS-MF由来frontendをPixel2用としてvendor化し、旧distribution名称を除去する
- [x] Pixel2 framebufferとgpio-key inputを自動選択してboot時にfrontendを起動する
- [x] frontendとADBの診断logをSTATE partitionへ保存する
- [x] 実機LCDでfrontend描画と90度回転を確認する
- [ ] 実機でfrontendのbutton mappingを確認する

## Connectivity

- [x] USB FunctionFS/configfs ADBをbring-up時の既定保守経路にする
- [ ] release imageではADB認証または明示opt-inを必須にする
- [x] USB Wi-Fi dongle検出とwpa_supplicant経路を実装する
- [x] ADB列挙とshellを実機検証する
- [ ] USB Wi-FiとSSHを実機検証する

## Image and hardware validation

- [x] MBR、Rockchip boot領域、boot FAT、state ext4、ROMS FATを生成する
- [x] image内のpartition境界、hash、SquashFS内容をhost検証する
- [ ] 複製SDでcold boot、LCD、input、audio、powerを実機検証する
- [ ] app-layer manifest/checksumを実機deploy単位で検証する
