
* プロジェクトのビルド

現在は prj/02_pcie_ddr4x4 のみ対応しています。時間ができたらすべて tcl スクリプト化する予定です。

```shell
cd prj/02_pcie_ddr4x4
vivado -mode batch -source project.tcl
```

```tcl

# コンパイル開始 
launch_runs synth_1 -jobs 8
wait_on_run synth_1


# bin/bit ファイルを生成
set_property STEPS.WRITE_BITSTREAM.ARGS.BIN_FILE true [get_runs impl_1]

# 配置配線
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1

# GUI を起動
start_gui

```

### 02_pcie_ddr4x4

* 全体構成
![](./prj/02_pcie_ddr4x4/images/pciex16_4xddr4_bram.png)

* アドレス割り当て
![](./prj/02_pcie_ddr4x4/images/pciex16_4xddr4_bram_addr.png)
	
* MIG OK
![](./prj/02_pcie_ddr4x4/images/pciex16_4xddr4_bram_ok.png)

* C OK
![](./prj/02_pcie_ddr4x4/images/pciex16_4xddr4_bram_testOK.png)


