# Pixel2 connectivity

Pixel2には内蔵Wi-Fiがないため、bring-up用の保守経路はUSB ADBを優先する。
USB Wi-Fi dongleは任意であり、認証情報がない場合はnetworkを起動しない。

## USB ADB

boot時にconfigfs gadget `plumos_pixel2`とFunctionFS `adb`を作成し、USB
VID:PID `18d1:4ee7`、product `plumOS Pixel2 ADB`として公開する。host側では
次のように接続する。

```sh
adb devices
adb shell
```

Pixel2には内蔵Wi-Fiがないため、設定がまだ存在しないfresh imageではADBを既定ON
にする。START > Network > NW Service > ADBで保存した明示的なON/OFFは次回bootで
最優先される。FE操作時に接続中のUSB gadgetを破壊しないため、変更は再起動後に
反映する。
設定画面へ到達できない場合は、SDカードのFAT32 user partition直下へ次の空fileを
置くとrecovery opt-inになる。

```text
plumos-enable-adb
```

ADBをOFFにすると保存設定を0にし、このrecovery markerも削除する。markerは明示
OFFより強いrecovery overrideであり、削除後は保存設定へ戻る。ADB有効中も
信頼できないhostへUSB接続しないこと。

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
まとめる。Wi-FiがOFFの場合はPIDとcmdlineを照合してmonitorを停止する。dongle未接続時の
定期pollingや無限retryは行わない。

stock kernel 5.10.198から採取した`r8188eu`に加え、V90Sで実機実績のある
RTL8811CU/RTL8821CU向け`8821cu.ko`を、Pixel2のstock kernel ABIに対して
再現可能にbuildしてSystemへ収録する。V90Sのkernel 4.9用module binaryは流用しない。

UGREEN AC650は接続直後に`0bda:1a2b Realtek DISK`として現れる場合がある。
Wi-Fi ON処理はこのIDに限って配下の`/dev/sr*`をbounded ejectし、
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
