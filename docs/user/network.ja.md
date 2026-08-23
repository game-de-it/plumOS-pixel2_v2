# ネットワーク接続とサービス

[English](network.md)

Pixel2はWi-Fiを内蔵していません。ネットワーク機能を利用するには対応USB Wi-Fi
アダプターを接続します。ADBは搭載していません。

## Wi-Fiへ接続する

1. Wi-Fiアダプターを挿します。
2. `START -> Network Settings`を開き、Wi-FiをONにします。
3. `Connect Wi-Fi`を開いてscanし、SSIDを選択します。
4. パスワードを入力して決定します。
5. 接続とIPv4取得を待ちます。Realtekのcold接続は20秒程度かかる場合があります。
6. `Network Settings -> Information`でIPアドレスを確認します。

実機確認済みの経路は次の通りです。

- 最初に`0bda:1a2b`として見える場合があるUGREEN AC650 / RTL8811CU。
  plumOSが自動的に`0bda:c811`へ切り替えます。
- `0bda:c811`または`0bda:c820`で直接認識するRTL8811CU/RTL8821CU。
- stock互換driverを使用する従来の`0bda:8179`アダプター。

SSIDとパスワードは、Wi-Fi接続とIP取得の両方が成功した後だけ保存されます。
確認済みアダプターは抜き差し後に保存済み接続を自動復旧します。Pixel2のUSBポートは
1つなので、充電器を接続する前にWi-Fiを抜いてください。

## ネットワークサービス

`START -> Network Settings -> NW Service`を開きます。新規イメージではSSHが
初期ON、その他の転送サービスは初期OFFです。下記`PIXEL2_IP`をInformation画面の
アドレスへ置き換えてください。

| サービス | 接続先 | ユーザー | 初期パスワード |
| --- | --- | --- | --- |
| SSH | `ssh root@PIXEL2_IP` | `root` | `plumos` |
| SFTP | `sftp root@PIXEL2_IP` | `root` | `plumos` |
| FTP | `ftp://PIXEL2_IP` | anonymous | なし |
| Samba | `smb://PIXEL2_IP/SDCARD` | `plumos` | `plumos` |

SSHとSFTPは実機内の同じパスワードを使用します。SSHから変更するには次を実行します。

```sh
/mnt/plumos/bin/plumos-ssh-password set
```

公開鍵認証は`/root/.ssh/authorized_keys`を使用します。パスワード、host key、
service選択は管理対象OSアップデート後も保持されます。

初期認証情報は公開情報です。信頼できるLANだけで使用し、SSHパスワードを変更して、
不要なサービスはOFFにしてください。
