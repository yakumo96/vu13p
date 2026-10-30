
## basebd

ここでは、ユーザーが二次開発に使える基本的な bd ファイルを提供します。


## ビルド 

`make` でプロジェクトをビルドし、ビルド完了後に `make genmcs` で mcs ファームウェアを生成します


## 書き込み

初回は JTAG で mcs ファームウェアを QSPI Flash に書き込みます。以降は JTAG を使わなくても構いません

## 起動

書き込みが完了したら OS を再起動します

## テスト



1. bram のテスト :  `cd driver/test_bram`
2. LED のテスト  :  `cd driver/test_led/`
3. XVC のテスト  :  DDDD 
