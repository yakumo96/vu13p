
## Step 1  FPGA のビルド

3 つの FPGA プロジェクトをまとめてビルドします（CPU が非力な場合は、```-j4``` オプションを付けずに個別にビルドしても構いません）。


```shell
cd vu13p/prj/01_icap_led
make -j4 tag=hello_masw
# CPU フル回転
```

ビルドが終わると、`~/alivu13p/prjs/golden_images` ディレクトリに 3 つのプロジェクトの xpr ファイルが生成され、カレントディレクトリには 3 つの bit ファイルが出力されます。


## Step 2  QSPI-FLASH ドライバのビルド

```shell
cd vu13p/prj/01_icap_led/driver/alex_qspi
make
```

カレントディレクトリに vu13p-fw というプログラムが生成されます。

```shell
➜  alex_qspi git:(main) ✗ ./vu13p-fw -h
usage: vu13p-fw [options]
 -d name    device to open (default: /dev/xdma0_user)
 -s slot    slot to program
 -r file    read flash to file
 -w file    write and verify flash from file
 -e         erase flash
 -b         boot FPGA from flash
 -t         hot reset FPGA
 -y         no interactive confirm
```

bit ファイルを QSPI Flash に書き込みます。

```shell
./vu13p-fw -w xxxxxxxx.bit -s 0 # 0 は書き込み先スロット 0（イメージ領域 0）を表す
./vu13p-fw -w xxxxxxxx.bit -s 1 # 1 は書き込み先スロット 1（イメージ領域 1）を表す
```

![](./images/QQ20231205000804.png)

## Step 3 動作検証

全体構成

![](images/pcie_icap-Page-4.drawio.png)


### 3.1 led テスト

```
cd driver/test_led
make 
# led 用の FPGA イメージが起動していることを確認
./led
# led の点滅が確認できる
```


### 3.2 bram テスト


```
cd driver/test_bram
make 
# bram 用の FPGA イメージが起動していることを確認
./bram
```


### 3.3 XVC テスト


プロジェクト example3 では、AXI debug bridge のアドレスを `0x2_0000` に設定しています。このアドレスは xdma ドライバ /dev/xdma0_xvc のデフォルトアドレス `0x4_0000` とは一致しません。

ホスト PC に xdma ドライバをインストールすると /dev/xdma0_xvc が作成されます。また、Vivado のインストールパスには xvc_pcie が含まれているので、Vivado のパスを PATH に追加済みであれば、任意のディレクトリで次を実行できます。

```shell
#xvc サービスを初期化
xvc_pcie -s TCP::10200 -d /dev/xdma0_xvc &
```

これで Vivado から XVC を使えるようになります。 


![](./images/opentarget.png)

![](./images/opentarget_2.png)

![](./images/opentarget_3.png)

* ILA の波形を表示するには probes file を読み込む必要があります。

* 補足: AXI bridge を接続した AXIL アドレスが 0x4_0000 ではなく 0x2_0000 の場合は、ドライバを再ビルド／再インストールし、xdma.ko を再ロードする必要があります。 

```shell
cd dma_ip_drivers/XDMA/linux-kernel/xdma/
sudo make xvc_bar_offset=0x20000
sudo make install 
```







