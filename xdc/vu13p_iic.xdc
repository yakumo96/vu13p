set_property  -dict {LOC BD8   IOSTANDARD  LVCMOS12 PULLUP true  SLEW SLOW DRIVE 8 }     [get_ports {main_iic_sda}  ] ; # inout
set_property  -dict {LOC BC12  IOSTANDARD  LVCMOS12 PULLUP true  SLEW SLOW DRIVE 8 }     [get_ports {main_iic_scl}  ] ; # inout
# log by xiongyw@20230822：bug here: iic main のピン割り当てがリバースエンジニアリングで正しく特定できていない。回路図モジュールを配置する際に bank を正しく選ばなかったためと思われる。正しいプルアップ電圧レベルを確保するため、当面は光ファイバ（QSFP）インターフェースの IIC を使用する
#
