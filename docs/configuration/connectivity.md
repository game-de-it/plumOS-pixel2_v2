# Pixel2 connectivity

Pixel2には内蔵Wi-Fiがないため、USB Wi-FiとSSH/SFTPを保守経路にする。ADB、
FunctionFS、USB Mode、FAT recovery markerは配布しない。USB Wi-Fi dongleは任意で、
認証情報がない場合はnetworkを起動しない。

単一USB portはWi-Fi優先のdual-role OTGである。cold boot時はstock OTGのまま
upstream VBUSを確認し、充電器がなければbounded host probeを行う。dongleを抜くと
kernel uevent経路が`otg_mode=otg`へ戻し、起動したまま充電器へ差し替えられる。
USB Wi-FiとUSB充電は同じ物理portを使うため同時利用できない。

## USB Wi-Fi

state partitionの次のpathへ通常の`wpa_supplicant.conf`を置く。

```text
/plumos/config/wpa_supplicant.conf
```

例:

```conf
ctrl_interface=/run/wpa_supplicant
update_config=0
country=JP

network={
    ssid="example"
    psk="replace-this"
}
```

boot時にUSB WLAN interfaceを検出し、`wpa_supplicant`とBusyBox `udhcpc`を
起動する。認証情報はimageやGitへ入れない。

Wi-Fiが保存設定でONの場合は`plumos-wifi-recovery`がBusyBoxのkernel uevent
monitorを1つ起動する。USB Wi-Fiの抜き差しで`wlan*`が再生成されたとき、または
RTL8821CUが`0bda:1a2b`、`0bda:c811`、`0bda:c820`として追加されたときだけ、3秒の
settle後に既存のbounded `plumos-network-control --wifi on`を1回実行し、driver load、
association、DHCP、network serviceを復元する。同時に届くUSB/net eventはlockで1回へ
まとめ、先行eventですでにIPv4接続済みならキューに残った後続eventを処理しない。
Wi-FiがOFFの場合はPIDとcmdlineを照合してmonitorを停止する。dongle未接続時の定期pollingや
無限retryは行わない。

stock kernel 5.10.198から採取した`r8188eu`に加え、V90Sで実機実績のある
RTL8811CU/RTL8821CU向け`8821cu.ko`を、Pixel2のstock kernel ABIに対して
再現可能にbuildしてSystemへ収録する。V90Sのkernel 4.9用module binaryは流用しない。

保存済みSSIDだけでは恒久的なUSB host所有権を与えない。起動時に`usb/online=1`、または
stock OTG中のRockchip USB2 PHY BVALIDがactiveならupstreamを優先する。それ以外では
hostへ切り替えてDWC2を再列挙するが、downstream deviceが現れなければ必ずOTGへ戻す。
Pixel2実機ではWi-Fi動作中もextconの`USB-HOST`が0だったため、extconは物理接続判定に
使わない。dongle removeは即時にOTGを解放するが、RTL8821CUの`0bda:1a2b` storage
identityだけは意図したeject/re-enumerationを壊さないよう、V90Sと同じ5秒の
mode-switch待ちを越える8秒後にdownstream不在を再確認してから解放する。常時pollingは
行わない。

OTGへ解放した後は、電源を供給しないUSB Wi-Fi dongleの再挿入をhardware eventだけでは
検出できない。FEのWi-Fi ON、SSID scan、接続操作は、adapter不在時に1回だけbounded host
probeを要求する。probeでadapterが見つからなければOTGへ戻るため、充電待受を壊さない。
boot worker、FE scan、recoveryからのprobeは単一lockで直列化する。hostへ切り替えた直後は
自然列挙を先に待ち、既に現れたdongleをDWC2 resetで切断しない。必要なreset中はtransition
markerを設定し、そのremove ueventを物理抜去として処理しない。

UGREEN AC650は接続直後に`0bda:1a2b Realtek DISK`として現れる場合がある。
Wi-Fi ON処理はこのIDに限って配下の`/dev/sr*`をSystem同梱の`/usr/bin/eject -s`で
bounded ejectし、
`0bda:c811`への再列挙を待って`8821cu`をloadする。直接`0bda:c811`または
`0bda:c820`で現れるadapterもmodule aliasから検出する。driver buildはUSB
autosuspendを無効、driver標準power savingを有効にしたV90Sと同じfeature
contractである。転送性能に応じたpower parameter変更は、実機A/B測定なしに
factory設定へ追加しない。

## SSH

V90S/MFと同じNW Service契約に従い、SSHはfresh imageで既定ON、dropbearを
port 22で起動する。初期accountは次の通り。

```text
user: root
password: plumos
```

初期passwordは公開情報なので、信頼できるLANだけで使い、不要ならFEからSSHを
OFFにする。`plumos-ssh-password set`で変更でき、salt付きSHA-512 hashだけを
`/mnt/plumos/config/ssh/shadow`へ端末ローカル保存する。既存passwordはOS更新や
service再起動で上書きしない。SFTPも同じaccountとport 22を使う。

公開鍵認証も併用できる。鍵は永続root homeの次のfileへ置く。

```text
/root/.ssh/authorized_keys
```

host keyは初回起動時に`/mnt/plumos/config/ssh`へ生成する。FTPはanonymous、
Sambaは`plumos / plumos`で`SDCARD` shareへ接続する。全serviceのON/OFFは
`/mnt/plumos/config/network/services.conf`へ保存し、次回bootでも復元する。
