set fpga_top_name [lindex $argv 0]
set output_path   [lindex $argv 1]

file mkdir ${output_path}

#1. プロジェクトを作成
create_project -force -part xcvu13p-fhgb2104-2L-e ${output_path}/${fpga_top_name}

#2. rtl ファイルと制約ファイルを追加
source ./tcl/add_sources.tcl
source ./tcl/add_constrs.tcl

#3. top を設定
set_property top ${fpga_top_name} [current_fileset]

#4. IP を追加
source ./ip/genip.tcl

#5. グローバル設定を変更
source ./tcl/config.tcl
