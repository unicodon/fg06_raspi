# usb gadget part



## init_usb
[init_usb](init_usb) は USB Gadget を ラズパイに生やすスクリプトです。

これを sudo 実行すると /dev/hidg0 が生えるはずです。

### 使い方
/dev/hidg0 から read してください。

### データフォーマット
BigEndian 4 バイト x 111 個がデータ単位になります。
先頭4バイトが0xFFnnnnnnnn の形式のスタートマーカです。
スタートマーカに続き4バイトのLEDの色データがLEDの個数 (110) 続きます。

