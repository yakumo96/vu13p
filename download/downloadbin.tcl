# create by masw@masw.tech using ChatGPT-4

# コマンドライン引数から bit ファイル名を取得
set bitname [lindex $argv 0]

# 絶対パスを取得
set abs_bitname [file normalize $bitname]

# ファイル名を抽出（パスと拡張子を除く）
set file_name [file rootname [file tail $abs_bitname]]

# 対応する bin ファイル名を生成
set binname "${file_name}.bin"

# 生成した bin ファイル名を表示
puts "Generated bin file name: $binname from $bitname"

# 新しい変数を使って変換
set load_command "up 0x00000000 \"$abs_bitname\""
set write_cfgmem_command "write_cfgmem -format bin -size 256 -interface SPIx4 -loadbit {$load_command} -file \"$binname\""

# 変換コマンドを実行
eval $write_cfgmem_command


#=========================================================================================================================
#=========================================================================================================================
#=========================================================================================================================
# Flash への書き込み



open_hw_manager
connect_hw_server -allow_non_jtag
open_hw_target
current_hw_device [get_hw_devices xcvu13p_0]
refresh_hw_device -update_hw_probes false [lindex [get_hw_devices xcvu13p_0] 0]
create_hw_cfgmem -hw_device [lindex [get_hw_devices xcvu13p_0] 0] [lindex [get_cfgmem_parts {mt25qu02g-spi-x1_x2_x4}] 0]
set_property PROGRAM.BLANK_CHECK  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
#set_property PROGRAM.ERASE        1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.CFG_PROGRAM  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.VERIFY       1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.CHECKSUM     0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
refresh_hw_device [lindex [get_hw_devices xcvu13p_0] 0]
set_property PROGRAM.ADDRESS_RANGE  {use_file} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.FILES $binname [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.PRM_FILE    {} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.UNUSED_PIN_TERMINATION {pull-none} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.BLANK_CHECK  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
#set_property PROGRAM.ERASE        1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.CFG_PROGRAM  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.VERIFY       1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
set_property PROGRAM.CHECKSUM     0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]
startgroup 
create_hw_bitstream -hw_device [lindex [get_hw_devices xcvu13p_0] 0] [get_property PROGRAM.HW_CFGMEM_BITFILE [ lindex [get_hw_devices xcvu13p_0] 0]]; program_hw_devices [lindex [get_hw_devices xcvu13p_0] 0]; refresh_hw_device [lindex [get_hw_devices xcvu13p_0] 0];
program_hw_cfgmem -hw_cfgmem [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcvu13p_0] 0]]





