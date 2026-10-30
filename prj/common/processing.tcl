# 環境変数を取得して返す関数。その変数が存在するかを判定し、存在しなければエラーを出して終了する
proc get_env {name} {
    # 環境変数が空の場合も許容するケースを追加 
    if {[info exists ::env($name)]} {
		set retx $::env($name)
		regsub {\s+$} $retx "" retx;# 変数末尾の空白を削除
        return $retx
    } else {
        puts "Error: Environment variable $name not found. VIVADO 要退出了"
        exit 1
    }
}
proc get_abspath {name} {
	set absolutePath [file normalize $name]
	return $absolutePath 
}

proc load_project {} {
    # makefile で export された環境変数からパラメータを取得する。その変数が存在するかを判定し、存在しなければエラーを出して終了する
    # 取得するパラメータは outdir top_name part。環境変数には一律で MKENV_ プレフィックスが付く点に注意
    # get_env 関数で環境変数を取得
    set outdir   [get_env MKENV_OUTDIR]
    set top_name [get_env MKENV_TOP_NAME]
    set part     [get_env MKENV_PART]
    set board    [get_env MKENV_BOARD]
	set absolutePath [file normalize $outdir/$top_name/$top_name.xpr]
    if {[file exists $absolutePath]} {
        open_project $absolutePath
    } else {
        create_project -force -part ${part} $top_name $outdir/$top_name 
    }
	set_property source_mgmt_mode None [current_project]
    if {$board ne ""} {
        set_property board_part $board [current_project]
    }
	set project_dir      [file normalize ${outdir}/${top_name}/${top_name}.data]
    if {[file isdirectory ${project_dir}] == 0} {
		file mkdir ${project_dir}
		puts "create file : $absolutePath "
	}
}


# ファイルをプロジェクトに追加し、更新する
proc add_filex {} {
    # ファイルを追加
    set outdir    [get_env MKENV_OUTDIR]
    set top_name  [get_env MKENV_TOP_NAME]
    set part      [get_env MKENV_PART]
    set board     [get_env MKENV_BOARD]
    set FILES_RTL [get_env MKENV_RTL_FILES]
    set PATHS_RTL [get_env MKENV_RTL_PATHS]
    set FILES_DEF [get_env MKENV_FILES_DEF]
    set FILES_BD  [get_env MKENV_FILES_BD]
    set FILES_TCL [get_env MKENV_FILES_TCL]
    set FILES_XDC [get_env MKENV_FILES_XDC]
    set FILES_SIM [get_env MKENV_FILES_SIM]
    set DEPS_LIB  [get_env MKENV_DEPS_LIB]
    set USE_OOC_SYNTHESIS [get_env MKENV_USE_OOC_SYNTHESIS]
    set USE_INCR_COMP     [get_env MKENV_USE_INCR_COMP]

    # リポジトリ DEPS_LIB を追加 
    if {$DEPS_LIB ne ""} {
		set liblist {}
		foreach filex $DEPS_LIB {
			set absolutefile [file normalize $filex]
			lappend liblist $absolutefile 
		}
		puts "$liblist"
        set_property ip_repo_paths $liblist [current_fileset]
        update_ip_catalog
    }

	# 各パスを走査
    if {$PATHS_RTL ne ""} {
		foreach path $PATHS_RTL {
			# パス内のすべての .v, .sv, .vhdl ファイルを取得
			set absolutePath [file normalize $path]
			#set rtl_files [glob -nocomplain -directory $absolutePath *.{v,sv,vhdl}]
			#add_files -norecurse -fileset sources_1 $rtl_files
			set rtl_files [glob -nocomplain -directory $absolutePath *.{v}]
			if {$rtl_files ne "" } {read_verilog     $rtl_files}
			set rtl_files [glob -nocomplain -directory $absolutePath *.{sv}]
			if {$rtl_files ne "" } {read_verilog -sv $rtl_files}
			set rtl_files [glob -nocomplain -directory $absolutePath *.{vhdl}]
			if {$rtl_files ne "" } {read_vhdl        $rtl_files}
		}
	}
    # RTL ファイルを追加
    if {$FILES_RTL ne ""} {
        # for ループで 1 ファイルずつ追加し、プロジェクトに同じファイルが既に存在するかを判定、存在すれば追加しない
		foreach filex $FILES_RTL {
			set absolutefile [file normalize $filex]
        	add_files -norecurse -fileset sources_1 $absolutefile 
		}
    }


    # マクロ定義ファイル FILE_DEF を追加し、global define verilog header に指定する。まず DEF が空かどうかを判定する
    if {$FILES_DEF ne ""} {
		foreach filex $FILES_DEF {
			set absolutefile [file normalize $filex]
			puts "add file $absolutefile "
            add_files -norecurse -fileset sources_1 $absolutefile 
            set_property file_type "Verilog Header" [get_files $absolutefile ]
            set_property is_global_include true     [get_files $absolutefile ]
        }
    }
    # TCL ファイルを追加
    if {$FILES_TCL ne ""} {
		foreach filex $FILES_TCL {
			set absolutefile [file normalize $filex]
			puts "source FILES_TCL file $absolutefile "
        	source $absolutefile 
		}
    }
    # 制約ファイルを追加
    if {$FILES_XDC ne ""} {
		foreach filex $FILES_XDC {
			set absolutefile [file normalize $filex]
			puts "add FILES_XDC file $absolutefile "
        	read_xdc $absolutefile 
		}
    }
    # BD ファイルを追加
    set project_bd_dir "$outdir/$top_name/$top_name.srcs/sources_1/bd/system"
    if {$FILES_BD ne ""} {
        # get_bd_define base が空なら作成し、そうでなければ読み込む
		#set tmp [get_bd_designs base]
        #if {$tmp eq ""} {
        #    create_bd_design -name base
        #}
        source $FILES_BD
        #create_root_design base
        # BD ファイルを保存して検証 
        #save_bd_design
        #validate_bd_design
    }
	#puts "=====================debug 2=============="
    # シミュレーションファイルを追加
    if {$FILES_SIM ne ""} {
		foreach filex $FILES_SIM {
			set absolutefile [file normalize $filex]
			puts "add file $absolutefile "
        	add_files -norecurse -fileset sim_1 $absolutefile 
		}
        # files_sim に tb.v があれば、シミュレーションの top に設定
        if {[regexp {tb.v} $FILES_SIM]} {
            set_property top tb [get_fileset sim_1]
        }
    }
	#puts "=====================debug 4=============="
    update_compile_order -fileset sources_1
    set_property top $top_name [current_fileset];# top を設定
    update_compile_order -fileset sources_1
    update_compile_order -fileset constrs_1
    update_compile_order -fileset sim_1
    # BD ファイルがあれば追加
}
# 

# ワークフローを作成
# プロジェクトを作成して必要なファイルを読み込んだ後、以下の関数をそれぞれ記述してワークフローを作成する
# 1. lint チェックのワークフローを作成し、RTL コードの規約準拠をチェックする。コード構造に問題があれば、以降のワークフローは実行しない
# 2. シミュレーションのワークフローを作成する。デフォルトでは Vivado 付属のシミュレータでシミュレーションする
# 3. 合成のワークフローを作成する。合成ワークフローでは合成のターゲットデバイスを指定する必要がある
# 4. インプリメンテーションのワークフローを作成する。ターゲットデバイスを指定する必要がある
# 5. パッケージングのワークフローを作成する。ターゲットデバイスを指定する必要がある
# 6. bit ファイル生成のワークフローを作成する。ターゲットデバイスを指定する必要がある
# 7. boot ファイル生成のワークフローを作成する。ターゲットデバイスを指定する必要がある

proc lint_flow {} {
    set part       [get_env MKENV_PART]
    set LINT_TOP   [get_env MKENV_LINT_TOP]
    set lint_ofile [get_env MKENV_LINT_OFILE]
	puts "synth_design -lint -mode default -top $LINT_TOP -part $part > $lint_ofile"
	synth_design -lint -mode default -top $LINT_TOP -part $part > $lint_ofile
}

proc export_siml {} {
    set sim_lib_map_path [get_env MKENV_SIM_LIB_MAP_PATH]
    set sim_dir          [get_env MKENV_SIM_DIR]
    set simulator        [get_env MKENV_SIMULATOR]
    set outdir           [get_env MKENV_OUTDIR]
    set top_name         [get_env MKENV_TOP_NAME]
    set ip_user_files_dir ${outdir}/${top_name}.ip_user_files
    export_simulation -lib_map_path ${sim_lib_map_path} \
                        -directory ${sim_dir} \
                        -simulator ${simulator} \
                        -ip_user_files_dir ${ip_user_files_dir} \
                        -ipstatic_source_dir ${ip_user_files_dir}/ipstatic \
                        -force -user_ip_compiled_libs 
}
proc synth_flow {} {
    set USE_OOC_SYNTHESIS [get_env MKENV_USE_OOC_SYNTHESIS]
    set MAX_OOC_JOBS      [get_env MKENV_MAX_OOC_JOBS]
    set outdir           [get_env MKENV_OUTDIR]
    set top_name         [get_env MKENV_TOP_NAME]
	set project_dir      [file normalize ${outdir}/${top_name}/${top_name}.data]
	reset_run   synth_1
    launch_runs -jobs $MAX_OOC_JOBS synth_1
    #launch_runs -jobs 20 synth_1
    wait_on_run synth_1
    open_run synth_1
	 
    report_timing_summary -file ${project_dir}/timing_synth.log
    #後続のインプリメンテーションで使う dcp ファイルを生成
    write_checkpoint -force ${project_dir}/${top_name}_synth.dcp
}

proc impl_flow {} {
    set USE_OOC_SYNTHESIS [get_env MKENV_USE_OOC_SYNTHESIS]
    set MAX_OOC_JOBS      [get_env MKENV_MAX_OOC_JOBS]
    set outdir           [get_env MKENV_OUTDIR]
    set top_name         [get_env MKENV_TOP_NAME]
	set project_dir      [file normalize ${outdir}/${top_name}/${top_name}.data]

    # dcp ファイルがあればインクリメンタルコンパイルを使用
    reset_run impl_1
    launch_runs -jobs ${MAX_OOC_JOBS} impl_1 
    wait_on_run impl_1 
    open_run impl_1
    report_timing_summary -warn_on_violation -file ${project_dir}/${top_name}_impl_timing.log
    write_checkpoint -force ${project_dir}/${top_name}_impl.dcp
	genbit_flow	
}
proc genbit_flow {} {
    set USE_OOC_SYNTHESIS [get_env MKENV_USE_OOC_SYNTHESIS]
    set MAX_OOC_JOBS      [get_env MKENV_MAX_OOC_JOBS]
    set outdir           [get_env MKENV_OUTDIR]
    set top_name         [get_env MKENV_TOP_NAME]
	set project_dir      [file normalize ${outdir}/${top_name}/${top_name}.data]

    # dcp ファイルがあればインクリメンタルコンパイルを使用
    open_run impl_1
    # タイムスタンプと現在の Linux ユーザー名を付けた bit ファイルを生成する。時刻とユーザー名の変数を事前に set する。時刻は分単位
    set username  [exec whoami]
    set prjtag    [get_env MKENV_PRJTAG]
    write_bitstream -force ${project_dir}/${top_name}_${prjtag}.bit
    write_bitstream -force ./${top_name}_${prjtag}.bit
}

proc runstep {} {
	launch_runs impl_1 -to_step write_bitstream -jobs 28
}
# runall
proc runall {argv} {
    load_project
    add_filex
    #export_siml
    synth_flow
    lint_flow
    impl_flow
}
proc openprj {argv} {
    load_project
	start_gui
}

proc export_lint {argv} {
    load_project
    add_filex
}

proc export_sim {argv} {
    load_project
    add_filex
    export_siml
}

proc export_synth {argv} {
    load_project
    add_filex
    synth_flow
    lint_flow
}

proc export_impl {argv} {
    load_project
    add_filex
    synth_flow
    lint_flow
    impl_flow
}

proc genmcs {argv} {
	set targetname [get_env MKENV_TARGET_NAME]
    set flashsize  [get_env MKENV_FLASH_SIZE]
    set goldenbit  [get_env MKENV_GBIT_FNAME]
    set offsetaddr [get_env MKENV_MCS_SADDR]
	set gbit       [get_abspath ${goldenbit}]
	write_cfgmem -force -format MCS -size ${flashsize} -interface SPIx4 -loadbit "up ${offsetaddr} ${gbit}" ${targetname}
}

proc multibootbin {argv} {
	set targetname [get_env MKENV_TARGET_NAME]
    set flashsize  [get_env MKENV_FLASH_SIZE]
    set goldenbit  [get_env MKENV_GBIT_FNAME]
    set mbootbit   [get_env MKENV_MBIT_FNAME]
    set mbootaddr  [get_env MKENV_MBADDR]
	set gbit       [get_abspath ${goldenbit}]
	set mbit       [get_abspath ${mbootbit}]
	write_cfgmem -force -format MCS -size ${flashsize} -interface SPIx4 -loadbit "up 0x00000000 ${gbit} up ${mbootaddr}  ${mbit}" ${targetname}
}

proc downloadbit {argv} {
    set HWTARGET [get_env MKENV_HWTARGET]
	set bitname [lindex $argv 1] 
	open_hw_manager
	connect_hw_server
	open_hw_target
	current_hw_device [get_hw_devices ${HWTARGET}]
	refresh_hw_device -update_hw_probes false [lindex [get_hw_devices ${HWTARGET}] 0]
	#program_hw_devices -file $bitname 
	if {[llength $argv] > 2} {
		# 引数の数が 2 より大きければ、3 番目の引数を変数に代入
		set ltxname [lindex $argv 2] 
		set_property PROBES.FILE ${ltxname} [get_hw_devices ${HWTARGET}]
	} else {
	    puts "no ltx file"
	}	
	set_property FULL_PROBES.FILE {} [get_hw_devices ${HWTARGET}]
	set_property PROGRAM.FILE ${bitname} [get_hw_devices ${HWTARGET}]
	program_hw_devices [get_hw_devices ${HWTARGET}]
	refresh_hw_device [lindex [get_hw_devices ${HWTARGET}] 0]
	close_hw_target
	disconnect_hw_server
}

proc implonly {argv} {
    load_project
    add_filex
	impl_flow 
}
proc genbitonly {argv} {
    load_project
    add_filex
	genbit_flow	
}

set function_name [lindex $argv 0]

# 指定された関数を呼び出す
if {[info commands $function_name] != ""} {
    eval $function_name {$argv}
} else {
    puts "Function $function_name not found"
}



