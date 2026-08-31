# Pixel2 PortMaster GPTokeYB終了回復

日付: 2026-08-31

対象source: `f86e4cea39ced5cfbd32a967d1dbd0af211ef773`

## 症状と原因

Rockbox操作中に多数のbuttonを押した後、Rockboxは画面上に残るが入力できず、
GPTokeYB2だけが終了していた。Rockboxのcontrol fileは
`guide`（Pixel2のFUNCTION）+`start`をGPTokeYB2内蔵終了hotkeyとして扱い、
監視対象を`pkill rockbox`で終了しようとする。

Pixel2の`pkill` shimは任意process名killを安全上拒否していた。GPTokeYB2は拒否後も
自分自身を終了するため、Rockbox本体だけが仮想input deviceなしで残った。これは
Rockbox固有のrendering問題ではなく、GPTokeYBの内蔵終了hotkeyを利用するPortMaster
appで共通に起こり得るsession終了問題である。

## 共通修正

adapter 54は次を実装する。

- `gptokeyb.pid` / `gptokeyb2.pid`と実行command lineを照合する。
- `pkill`呼出元の親process chainを最大8段検査し、wrapperが所有するGPTokeYBからの
  呼出しだけを認証する。
- 認証済みの終了要求を、既存のPID、start time、launcher path照合を持つ
  `plumos-portmaster-port-stop stop`へ変換する。
- 通常port shellからの任意`pkill`は従来どおり拒否する。

これにより個別のprocess名を許可せず、GPTokeYBを利用するPortMaster appを共通session
単位で終了・回収する。

## 検証

動的Linux fixtureは所有GPTokeYB経路がsession process groupを停止することと、同じ
`pkill rockbox`を通常port shellから呼ぶと拒否されることを確認した。

```text
portmaster_pixel2_gptokey_exit=result-ok
portmaster_pixel2_runtime=result-ok
```

strict PortMaster buildとstrict app-layer buildも合格した。adapter 54のmanaged deltaを
実機へ原子的にdeployし、PortMaster component 180件、app-layer全体11,334件のchecksumを
確認した。更新前後でRockboxの`.resume.cfg` SHA-256は同一だった。

```text
b2074cfdd05b3031670e207ea1f84c383cb0ac089c7a95ea7989349f78b2bbe5
```

実機でRockboxとGPTokeYB2の同時起動をprocess単位で確認し、利用者が
`FUNCTION+START`後のFE復帰を目視確認した。終了直後はWi-Fi SSHが到達不能だったため、
process・mountの最終readbackは取得していない。この部分はhostのsession cleanup fixtureと、
FE復帰の物理観測を区別して扱う。

