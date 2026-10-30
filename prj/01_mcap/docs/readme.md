
# Tandem PCIe について

PCI Express® はプラグアンドプレイのプロトコルです。つまり電源投入時に PCIe® ホストがシステムをエニュメレーションします。この過程でホストは各デバイスから要求されるアドレスサイズを読み取り、デバイスにベースアドレスを割り当てます。したがって、ホストが PCIe インターフェースを問い合わせる時点で、それらのインターフェースは準備完了状態でなければならず、そうでなければベースアドレスが割り当てられません。PCI Express 仕様では、システム電源が安定してから 100 ms 後に PERST# がデアサートされ、PERST# のデアサート後 20 ms 以内に PCI Express ポートがリンクトレーニングの準備を完了していなければならないと規定されています。これは一般に 100 ms 起動時間要件と呼ばれます。

「Tandem Configuration」（タンデムコンフィギュレーション）は 2 段階の方式を用いて、IP が PCI Express 仕様に記載されたコンフィギュレーション時間要件を満たせるようにします。この技術は以下のような複数のユースケースをサポートします:

* 「Tandem PROM」（タンデム PROM）: フラッシュから 2 段階の単一ビットストリームをロードします。
* 「Tandem PCIe」（タンデム PCIe）: フラッシュから第 1 段階のビットストリームをロードし、第 2 段階のビットストリームは PCIe リンク経由で MCAP に渡します。
* 「Tandem PCIe with Field Updates」（フィールドアップデート対応タンデム PCIe）: タンデム PCIe の初期コンフィギュレーション完了後、PCIe リンクを
維持したままユーザーデザイン全体を更新します。更新領域（フロアプラン）とデザイン構造はあらかじめ定義されており、Tcl スクリプトも提供されています。

* タンデム Dynamic Function eXchange: 比較的よく使われるユースケースで、任意のサイズ・任意の数の動的領域に対し、タンデムコンフィギュレーションの後に
Dynamic Function eXchange (DFX) を行います。
* 「DFX over PCIe」（PCIe ベースの DFX）: 標準コンフィギュレーションの後に DFX を行い、部分ビットストリームの転送に PCIe / MCAP を使用します。

ユースケースに応じて「Tandem Configuration or Dynamic Function eXchange」（タンデムコンフィギュレーションまたは Dynamic Function
eXchange）オプションを変更します:
* 「Tandem PROM」はタンデム PROM のユースケースに使用します。
* 「Tandem PCIe」はタンデム PCIe、またはタンデムと Dynamic Function eXchange を組み合わせるユースケースに使用します。
* 「Tandem PCIe with Field Updates」は、あらかじめ定義された「Field Updates」ユースケース専用です。
* 「DFX over PCIe」は、「Tandem Configuration」を有効にせずに DFX 用の MCAP リンクを有効にするために使用します。


## 1 原理

Tandem PCIe は従来の単一ビットストリームを 2 つの部分に分割します。第 1 部分には必要最小限の PCIe 関連ロジックのみ（Stage 1）、第 2 部分にはユーザーアプリ関連のロジック（Stage 2）が含まれます。


下図のとおりです：

![](./images/tandem_pcie.png)


ボード提供側は出荷時に Stage 1 を設定し、そのロジックを dcp ファイルとして固定（ロック）しておきます。ユーザーは同じ PCIe モジュールを使い、mcap で Stage 2 を書き込むだけで済みます。

### 1. ビルド後、tandem1 を Flash に書き込む

```shell
make #ビルド
```


ビルドすると 2 つの bit ファイルが生成されます：

![](./images/tandem_find_two_tandem.png)



bin ファイルを生成した後、JTAG で bin ファイルを flash に書き込みます (02g_x1_x2_x4)


```shell
vu13p-fw -w xxxxxxxxxx_tandem1.bit
#main_stage の内容が変わらなければ、この手順は 1 回だけ実行すれば良い
```

書き込み完了後、PCIe ボードをコールドリブートしてください。

#### tandem2.bit の書き込み

本ドキュメントは Ubuntu20 上の linux5.15 環境でのみ検証済みです。まず依存パッケージをインストールします。

```shell
# 1. 依存パッケージ
sudo apt install zlib1g
# 2. pcieutils のインストール
cd driver/driver/an64761/pcieutils
cat get_pcieutils.sh
source get_pcieutils.sh
# 3. mcap のインストール
cd ../Linux
cat compile.sh
source compile.sh
./mcap -h
```

インストールできたら確認します：
![](./images/tandem_linux_test.png)






