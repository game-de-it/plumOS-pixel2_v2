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

stock kernel 5.10.198から採取できた外付けmoduleは`r8188eu`に限られる。
最終plumOS kernelでは、候補dongleを決めたうえでin-tree USB WLAN driverと
必要なfirmwareを有効にする。

## SSH

次のfileが存在する場合だけdropbearをport 22で起動する。NW ServiceのSSH/SFTP
toggleは保存されるが、鍵がない場合は意図的にlistenしない。

```text
/plumos/root/.ssh/authorized_keys
```

password loginとroot password loginは無効である。host keyは初回boot時に
`/plumos/ssh`へ生成する。
