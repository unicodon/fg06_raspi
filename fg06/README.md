# usb gadget part



## init_usb
[init_usb](init_usb) は USB Gadget を ラズパイに生やすスクリプトです。

使うときは chmod +x で実行属性をつけておいてください。
これを sudo 実行すると /dev/hidg0 が生えるはずです。

### 使い方
/dev/hidg0 から read してください。

```cat /dev/hidg0 | od -x```
で動作確認するとよいでしょう

### データフォーマット
BigEndian 4 バイト x 111 個がデータ単位になります。
先頭4バイトが0xFFnn0000 の形式のヘッダです。
nn: LEDの数です。
他のデータには0xFFが現れないようにしているので、0xFFをスタートマーカとして扱ってください。
スタートマーカに続き4バイトのLEDの色データがLEDの個数続きます。

## fg06.service
[fg06.service](fg06.service) は systemd 用の service ファイルです。
起動時に USB Gadget を自動的に生やすのに使います。

### 使い方
fg06.service を /etc/systemd/system 以下に配置してください。
また init_usb は /usr/bin/init_usb においてある前提です。

#### service の有効化
sudo systemctl enable fg06.service
で起動時に init_usb が実行されるようになります。




