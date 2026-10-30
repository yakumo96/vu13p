
## basebd

ここでは、ユーザーが独自開発のベースとして使える基本的な bd ファイルを提供します。


## ビルド 

`make` でプロジェクトをビルドし、ビルド完了後に `make genmcs` で mcs ファイルを生成します。


## 書き込み

初回は JTAG で mcs ファイルを QSPI Flash に書き込みます。以降は JTAG は不要です。

## 起動

書き込みが完了したらホスト PC を再起動します。

## テスト



1. bram のテスト :  `cd driver/test_bram`
2. LED のテスト  :  `cd driver/test_led/`
3. XVC のテスト  :  [docs/readme.md](./docs/readme.md) の「3.3 XVC テスト」を参照
