
# 使い方

```shell
unzip ./hdl_2023r2_2156ac7.zip 
vim ./hdl-main/scripts/adi_env.tcl 
// set required_vivado_version "2023.2" をお使いのバージョンに変更してください
set required_vivado_version "2024.1"

cd ./hdl-main/library/ 
make -j8
```
