
# 使い方

##  pciutils を使って mcap ツールをインストール
  * `cd driver/an64761/pcieutils`
  * `./get_pciutils.sh`
  * `cd driver/an64761/Linux`
  * `./compile.sh` で mcap ツールを取得

## mcap で bit ファイルを書き込む

```
➜  Linux git:(main) ✗ lspci | grep Xilinx
03:00.0 Serial controller: Xilinx Corporation Device 903f
➜  Linux git:(main) ✗ sudo ./mcap -x 903f
Xilinx MCAP device found
➜  Linux git:(main) ✗ sudo ./mcap -x 0x903f -p ../../../../../alivu13p/prj/02_pcie_ddr4x3_streamloop/alivu13p_top_notag_tandem2.bit 
Xilinx MCAP device found
FPGA Configuration Done!!
```

## ドライバのインストール

```
cd dma_ip_drivers/XDMA/linux-kernel/XDMA
make 
sudo make install 
cd dma_ip_drivers/XDMA/linux-kernel/tests
sudo ./load_driver.sh
```


## テスト

### DDR 速度テスト

```
cd driver/ddr4test/ 
make 
➜  ddr4test git:(main) ✗ make
./testddr4x3
写入16GB内存的速度为: 6.442451 GB/s
读出16GB内存的速度为: 6.442451 GB/s
正在比对0x000000000 ~ 0x040000000中的内容, 比对完成
正在比对0x040000000 ~ 0x080000000中的内容, 比对完成
正在比对0x080000000 ~ 0x0c0000000中的内容, 比对完成
正在比对0x0c0000000 ~ 0x100000000中的内容, 比对完成
正在比对0x100000000 ~ 0x140000000中的内容, 比对完成
正在比对0x140000000 ~ 0x180000000中的内容, 比对完成
正在比对0x180000000 ~ 0x1c0000000中的内容, 比对完成
正在比对0x1c0000000 ~ 0x200000000中的内容, 比对完成
正在比对0x200000000 ~ 0x240000000中的内容, 比对完成
正在比对0x240000000 ~ 0x280000000中的内容, 比对完成
正在比对0x280000000 ~ 0x2c0000000中的内容, 比对完成
正在比对0x2c0000000 ~ 0x300000000中的内容, 比对完成
```

※ 上記はテストプログラムの実際の出力（中国語メッセージ）をそのまま掲載しています。意味はそれぞれ「16GB メモリへの書き込み速度: … GB/s」「16GB メモリからの読み出し速度: … GB/s」「0x… ~ 0x… の内容を比較中、比較完了」です。

### ila テスト

```
#xvc サービスを初期化
sudo xvc_pcie -s TCP::10200 -d /dev/xdma0_xvc &
```
これで Vivado から XVC を使えるようになります。 


![](../01_basebd/docs/images/opentarget.png)

![](../01_basebd/docs/images/opentarget_2.png)

![](../01_basebd/docs/images/opentarget_3.png)

* ILA の波形を表示するには probes file を読み込む必要があります。

* 補足: AXI bridge を接続した AXIL アドレスが 0x4_0000 ではなく 0x2_0000 の場合は、ドライバを再ビルド／再インストールし、xdma.ko を再ロードする必要があります。 

```shell
cd dma_ip_drivers/XDMA/linux-kernel/xdma/
sudo make xvc_bar_offset=0x20000
sudo make install 
```

### DMA ループバックテスト


