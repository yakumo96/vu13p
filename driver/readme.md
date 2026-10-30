**注意**
xdma には Linux カーネルのバージョン要件があり、5.15 を超えてはいけません。`uname -a` コマンドで Linux カーネルのバージョンを確認してください。
2023年3月の実機テストでは、xdma は Ubuntu 22.04 に対応していませんでした。 


## DMA ドライバのインストール

1. まず Xilinx 公式リポジトリを取得します

```bash
git submodule update --init --recursive
```

2. make 

```bash
cd ./dma_ip_drivers/XDMA/linux-kernel/xdma
make
sudo make install 
```

3.  ドライバのロード

```bash
cd ./dma_ip_drivers/XDMA/linux-kernel/tests
sudo ./load_driver.sh
```

以下のメッセージが表示されれば、ドライバは正常にロードされています

```bash
➜  tests git:(master) ✗ sudo ./load_driver.sh
interrupt_selection .
Loading driver...insmod xdma.ko interrupt_mode=1 ...

The Kernel module installed correctly and the xmda devices were recognized.
DONE
```

この状態で `ls /dev/xdma*` を実行すると、ドライバが正常にロードされていることを確認できます

```bash
➜  tests git:(master) ✗ ll /dev/xdma*
crw-rw-rw- 1 root root 511, 36 8月   8 23:51 /dev/xdma0_c2h_0
crw-rw-rw- 1 root root 511, 37 8月   8 23:51 /dev/xdma0_c2h_1
crw-rw-rw- 1 root root 511, 38 8月   8 23:51 /dev/xdma0_c2h_2
crw-rw-rw- 1 root root 511, 39 8月   8 23:51 /dev/xdma0_c2h_3
crw-rw-rw- 1 root root 511,  1 8月   8 23:51 /dev/xdma0_control
crw-rw-rw- 1 root root 511, 10 8月   8 23:51 /dev/xdma0_events_0
crw-rw-rw- 1 root root 511, 11 8月   8 23:51 /dev/xdma0_events_1
crw-rw-rw- 1 root root 511, 20 8月   8 23:51 /dev/xdma0_events_10
crw-rw-rw- 1 root root 511, 21 8月   8 23:51 /dev/xdma0_events_11
crw-rw-rw- 1 root root 511, 22 8月   8 23:51 /dev/xdma0_events_12
crw-rw-rw- 1 root root 511, 23 8月   8 23:51 /dev/xdma0_events_13
crw-rw-rw- 1 root root 511, 24 8月   8 23:51 /dev/xdma0_events_14
crw-rw-rw- 1 root root 511, 25 8月   8 23:51 /dev/xdma0_events_15
crw-rw-rw- 1 root root 511, 12 8月   8 23:51 /dev/xdma0_events_2
crw-rw-rw- 1 root root 511, 13 8月   8 23:51 /dev/xdma0_events_3
crw-rw-rw- 1 root root 511, 14 8月   8 23:51 /dev/xdma0_events_4
crw-rw-rw- 1 root root 511, 15 8月   8 23:51 /dev/xdma0_events_5
crw-rw-rw- 1 root root 511, 16 8月   8 23:51 /dev/xdma0_events_6
crw-rw-rw- 1 root root 511, 17 8月   8 23:51 /dev/xdma0_events_7
crw-rw-rw- 1 root root 511, 18 8月   8 23:51 /dev/xdma0_events_8
crw-rw-rw- 1 root root 511, 19 8月   8 23:51 /dev/xdma0_events_9
crw-rw-rw- 1 root root 511, 32 8月   8 23:51 /dev/xdma0_h2c_0
crw-rw-rw- 1 root root 511, 33 8月   8 23:51 /dev/xdma0_h2c_1
crw-rw-rw- 1 root root 511, 34 8月   8 23:51 /dev/xdma0_h2c_2
crw-rw-rw- 1 root root 511, 35 8月   8 23:51 /dev/xdma0_h2c_3
crw-rw-rw- 1 root root 511,  0 8月   8 23:51 /dev/xdma0_user
crw-rw-rw- 1 root root 511,  2 8月   8 23:51 /dev/xdma0_xvc
```

ただし所有者が root のため、プログラムで PCIE にデータを読み書きするたびに sudo が必要になります。sudo なしでプログラムを実行できるよう、キャラクタデバイスの所有者を自分に変更することを推奨します。

```bash
echo $USER
echo $GROUP
sudo chown $USER:$GROUP /dev/xdma* 
```

4. 起動時にドライバを自動ロード

xdma.ko ドライバが正しくロードされているか確認します:

```zsh
ll /usr/lib/modules/$(uname -r)/extra # ロード済みかどうか確認
vim /etc/modules-load.d/xdma.conf
```
Linux の起動時にドライバを自動ロードするには、以下の手順で `/usr/lib/modules/$(uname -r)/extra/xdma.ko` ドライバを自動ロードするよう設定できます：

**モジュール設定ファイルの作成：** `/etc/modules-load.d/` ディレクトリに新しいファイル（例: `xdma.conf`）を作成し、起動時に必要なモジュールをロードするようシステムに指示します。

   ```
   sudo vim /etc/modules-load.d/xdma.conf
   ```

**ドライバモジュール名の追加：** ファイルにドライバモジュール名 `xdma` を、1 行に 1 モジュールずつ記述します。

   ```
   xdma
   ```

   これにより、起動時に `xdma.ko` ドライバをロードするようシステムに指示されます。

**ファイルの保存：** ファイルを保存してテキストエディタを閉じます。新たに追加した xdma ドライバを `.dep` ファイルに反映します。

```
sudo depmod
```

**システムの再起動：** システムを再起動すると、設定ファイルの内容に従ってドライバモジュールが自動的にロードされます。

注意事項：
- ドライバファイル `xdma.ko` が確かに `/usr/lib/modules/$(uname -r)/extra/` ディレクトリにあることを確認してください。ドライバファイルのパスが正しいことを確認してください。
- この方法はほとんどの Linux ディストリビューションで使えますが、一部のディストリビューションでは多少異なる場合があります。問題が発生した場合は、お使いのディストリビューションのドキュメントを参照してください。
- 場合によっては、ロード後のドライバが正しい権限とアクセス制御を持つよう、udev ルールなどの設定が必要になることもあります。
- log by masw@20230821: ハマりました。最近システムが Linux カーネルを 5.15.0-78-generic から 5.15.0-79-generic に自動アップグレードしたところ、xdma がどうしてもロードできなくなり、仕方なく 5.15.0-78 に戻したら一発で OK！ここに記録しておきます！

操作は慎重に行い、重要なデータは必ずバックアップし、作業前にシステムとドライバの構成を十分に理解しておいてください。

4. 起動後のデバイスのデフォルト所有者を変更

```
sudo vim /etc/udev/rules.d/99-xilinx_xdma.rules
```

```
KERNEL=="xdma*", MODE="0777", OWNER="masw", GROUP="masw" # 所有者を適宜変更すること
```

```
udevadm control --reload-rules
```


