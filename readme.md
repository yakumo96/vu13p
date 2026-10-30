
### ali-vu13p：国産 VU13P アクセラレータカード資料（日本語版）

> 本リポジトリは [maswx/vu13p](https://github.com/maswx/vu13p) をフォークし、コメントとドキュメントを日本語に翻訳したものです。オリジナルの作者およびコントリビューターに感謝します。

* 質問がある場合は [issue](https://github.com/maswx/vu13p/issues) に書き込んで記録してください。
* サードパーティのリポジトリを参照しています。すべてのリポジトリを取得するには、コマンド ` git clone --recurse-submodules https://github.com/maswx/vu13p.git ` を使用してください。


### 使い方

#### [00 pin uart](./prj/00_pin_uart/README.md)

* 自分でしゃべって自己紹介してくれるピン！
* リバースエンジニアリング。わかる人にはわかる（DDDD）
* 検証済み ✔

#### [01_basebd](./prj/01_basebd/readme.md)

* 基本的な BD とプロジェクトスクリプトの example
  * Makefile スクリプトと共通プロジェクト tcl ファイル
  * GPIO で LED を駆動

#### [01_basebd_golden](./prj/01_basebd_golden/readme.md)

* ボードの基本 golden イメージ。QSPI に書き込んで固定化できる
  * GPIO で LED を駆動
  * BRAM テストプログラム
  * QSPI ドライバ
  * AXI XVC example

#### [01_basebd_multiboot](./prj/01_basebd_mb/readme.md)

* ボードの基本 multiboot イメージ。QSPI の multiboot 領域に書き込んで固定化できる
  * QSPI ドライバのみを含む


#### [01 mcap_goldmb](./prj/01_mcap_goldmb/docs/readme.md)

* PCIE 経由で MCAP を使ってファームウェアをダウンロードする方法を紹介
* QSPI から Tandem PCIE main stage を起動し、PCIE から残りの bit を更新
  * tcl で複数のコンパイル領域に分割（検証済み ✔）
  * pcie-mcap で bit をダウンロード（検証済み ✔）

#### [02 pcie ddr4x4](./prj/02_pcie_ddr4x4/readme.md)

* ボード搭載の DDR4 メモリ 4 系統をテスト
* PCIE 関連ドライバの紹介
  * 16GB DDR4 の読み書き／速度測定[サンプル](./prj/02_pcie_ddr4x4/driver/ddr4test/src/main.c)（検証済み ✔）
  * シンプルなホスト側割り込みの[example](./prj/02_pcie_ddr4x4/driver/intptest/src/xdma0_events_0_example.c)（検証済み ✔）

#### [03 10G/25G UDP](./prj/03_10g25g_udp/README.md)

* `make` でプロジェクトをビルド！
* Alex の成果をベースにしています。Alex 最高（YYDS）！
* UDP 疎通済み！
* プロジェクト検証済み ✔

#### [04 100G Corundum](./prj/04_100G_corundum/readme.md)

* テイクオフ
* プロジェクト検証済み ✔

#### [05 UDP + Gnuradio]

* 開発中

#### [06 PCIe + MATLAB]

* TODO

#### [07 QDMA + DPDK]

* TODO

#### [08 PCIe + DSLogic]

* 長期計画
* 不定期更新



### 謝辞

* 一部の DDR4 制約を提供してくれた「章鱼哥」さんに感謝します
* 一部のリバースエンジニアリング資料を提供してくれた「冻结旋律」さんに感謝します

