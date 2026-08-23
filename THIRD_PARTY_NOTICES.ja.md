# サードパーティー通知

この文書はplumOS Pixel2が配布する主なthird-partyおよびvendor由来componentを記録します。
正確なrevisionはbuild recipe、component manifest、
`package/licenses-pixel2/runtime-license-index.tsv`を正とします。

plumOSが作成した部分はrepositoryのMIT Licenseに従います。MIT Licenseはthird-party
binary/source、font、firmware、ROM、BIOS、利用者contentを再licenseしません。releaseへ
ROMとgame BIOSを含めません。

English counterpart: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Platform境界

GKD Pixel2のRockchip boot prefix、stock Linux 5.10.198 kernel、runtime DTB、stock
initramfs、選択module・firmwareはhardware互換substrateです。vendor由来materialは元の
条件を保持し、plumOS MIT Licenseの対象ではありません。releaseは登録済みhashと
`pixel2-stock-vendor-runtime-NOTICE.txt`を収録します。

## 同梱runtime family

| runtime family | upstream・origin | 収録する根拠 |
| --- | --- | --- |
| plumOS FE・hardware service | plumOS Pixel2 repository | `plumOS-MIT.txt`、component manifest、font notice |
| System | plumOS、BusyBox、Debian runtime package、stock-kernel module/firmware | System `/usr/share/licenses`、System manifest、stock vendor notice |
| RetroArch | <https://github.com/libretro/RetroArch> | source treeの`COPYING`、component manifest |
| libretro core | `docker/pixel2-tools/libretro-core-recipes.tsv`の固定repo/ref | upstream license-bearing file、core manifest |
| PicoArch・SDL互換layer | <https://github.com/shauninman/picoarch>とSDL互換project | PicoArch・SDL license text |
| SA | PCSX-ReARMed、steward-fu/nds + DraStic、PPSSPP、OpenBOR | upstream notice/license、固定ref/hash、SA manifest |
| PICO-8 | 利用者提供proprietary runtime | PICO-8 binaryとgame dataはreleaseへ含めない |
| Pyxel/Python | CPython、Pyxel、pygame、NumPy、Pillowなど | Python license、package metadata内license |
| PortMaster | <https://github.com/PortsMaster/PortMaster-GUI>と互換library | upstream `PortMaster/licenses`、個別notice |
| File Manager | <https://github.com/LoveRetro/NextCommander> | 固定revision、component manifest、upstream-license-status notice |
| Music Player | plumOS application、miniaudio | plumOS MIT、miniaudio license material |
| Network service | BusyBox、Dropbear、OpenSSH SFTP、Samba、dosfstoolsなど | component manifest、package/source license material |
| Font・graphical asset | Noto CJK、DejaVu、RetroArch assets、theme | componentと一緒に保持するfont/asset license |

## 明示するupstream license状態

固定したNextCommander sourceには独立したLICENSE、COPYINGまたは同等の許諾fileが
ありません。専用noticeはこれを`NOASSERTION`として記録するもので、plumOS MIT Licenseを
代用しません。

steward-fu/nds統合sourceはLGPL-2.1ですが、DraStic executableは別作者のclosed software
です。統合sourceのLGPLはDraStic本体を再licenseしません。公式release README、asset
hash、component manifest、専用noticeを保持します。plumOS Pixel2は他plumOS handheldと
同じ文書化済みinclusion policyを採用しますが、追加の権利を主張しません。

## 配布前checklist

- `LICENSE`、`NOTICE.md`、英日notice、runtime license index、component manifest、
  exact source refを保持する
- final app-layer/Systemへ`audit-pixel2-license-bundle.sh`を実行する
- strict content gateと圧縮imageのchecksumを検証する
- ROM、BIOS、save/state、credential、personal key、PICO-8、利用者install Ports/Pyxelを除外する
- source build componentに必要なtagged repository source、recipe、patchを公開する
- NextCommander/DraSticをMIT等と誤記せず専用status noticeを残す
