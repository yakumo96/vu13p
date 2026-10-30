//========================================================================
//        author   : masw
//        creattime: Sun 19 Mar 2023 04:40:04 PM CST
//========================================================================
//
//
//
//  Model: GPT-4
//  
// Q Verilog で FIR フィルタを書いてください。入出力ポートのビット幅は 16bit、係数は全部で 33 個、各係数のビット幅も 16bit で、係数のビット幅はポートで設定できるようにしてください
//
// A  ご要望に基づき、以下は Verilog による FIR フィルタの実装です。16 ビット幅の入出力ポートと、16 ビット幅の係数 33 個を持ち、係数のビット幅はポートで設定できます。
// 
// Q 入力信号 data_in は符号付き数なので、乗算記号 * をそのまま使うのは正しくありません。乗算専用のサブモジュールを追加するか、xilinx ultrascale plus の乗算器プリミティブを呼び出すべきです。また加算ツリーが長すぎるので、加算ツリーにレジスタを挿入してパイプライン化すべきです
//
// A ご提案はとても妥当です。符号付き数の乗算には専用の乗算サブモジュールを使えます。ここでは Xilinx UltraScale+ FPGA ベースの符号付き乗算器プリミティブ DSP48E1 を使った実装を示します。
//
// Q 累算器で加算 4 回ごとに 1 段レジスタを入れたいです。
//
// A ご要望に基づき、累算器で加算 4 回ごとにレジスタを 1 つ追加できます。以下は修正後の FIR フィルタモジュールです：


//  verilog
module fir_core (
    input wire         clk     , 
    input wire         rst     , 
    input wire  [15:0] data_in ,
	input       [15:0] coeff_00,
	input       [15:0] coeff_01,
	input       [15:0] coeff_02,
	input       [15:0] coeff_03,
	input       [15:0] coeff_04,
	input       [15:0] coeff_05,
	input       [15:0] coeff_06,
	input       [15:0] coeff_07,
	input       [15:0] coeff_08,
	input       [15:0] coeff_09,
	input       [15:0] coeff_10,
	input       [15:0] coeff_11,
	input       [15:0] coeff_12,
	input       [15:0] coeff_13,
	input       [15:0] coeff_14,
	input       [15:0] coeff_15,
	input       [15:0] coeff_16,
	input       [15:0] coeff_17,
	input       [15:0] coeff_18,
	input       [15:0] coeff_19,
	input       [15:0] coeff_20,
	input       [15:0] coeff_21,
	input       [15:0] coeff_22,
	input       [15:0] coeff_23,
	input       [15:0] coeff_24,
	input       [15:0] coeff_25,
	input       [15:0] coeff_26,
	input       [15:0] coeff_27,
	input       [15:0] coeff_28,
	input       [15:0] coeff_29,
	input       [15:0] coeff_30,
	input       [15:0] coeff_31,
	input       [15:0] coeff_32,
    output wire [15:0] data_out,
	output wire [63:0] testvec  //ロジックアナライザに送るテストベクタ
);

    wire [15:0] coeff[32:0];

	assign coeff[00] = coeff_00;
	assign coeff[01] = coeff_01;
	assign coeff[02] = coeff_02;
	assign coeff[03] = coeff_03;
	assign coeff[04] = coeff_04;
	assign coeff[05] = coeff_05;
	assign coeff[06] = coeff_06;
	assign coeff[07] = coeff_07;
	assign coeff[08] = coeff_08;
	assign coeff[09] = coeff_09;
	assign coeff[10] = coeff_10;
	assign coeff[11] = coeff_11;
	assign coeff[12] = coeff_12;
	assign coeff[13] = coeff_13;
	assign coeff[14] = coeff_14;
	assign coeff[15] = coeff_15;
	assign coeff[16] = coeff_16;
	assign coeff[17] = coeff_17;
	assign coeff[18] = coeff_18;
	assign coeff[19] = coeff_19;
	assign coeff[20] = coeff_20;
	assign coeff[21] = coeff_21;
	assign coeff[22] = coeff_22;
	assign coeff[23] = coeff_23;
	assign coeff[24] = coeff_24;
	assign coeff[25] = coeff_25;
	assign coeff[26] = coeff_26;
	assign coeff[27] = coeff_27;
	assign coeff[28] = coeff_28;
	assign coeff[29] = coeff_29;
	assign coeff[30] = coeff_30;
	assign coeff[31] = coeff_31;
	assign coeff[32] = coeff_32;



    // 内部信号の定義
    reg [15:0] data_buffer [32:0];
    wire [15:0] mult_result [32:0];
    reg  [15:0] accum_result_stage1 [7:0];
    reg  [15:0] accum_result_stage2 [1:0];
    reg  [15:0] final_result;
    reg  [15:0] mult_result_d1;
    reg  [15:0] mult_result_d2;

    // 符号付き乗算器サブモジュールのインスタンス化
    generate
    genvar i;
		// DSP48E1 プリミティブで乗算を行う
        for (i = 0; i < 33; i = i + 1) begin
			libv_base_smult #(16,16,16) libv_base_smult_inst(
				.a(data_buffer[i]), 
				.b(coeff[i]      ), 
				.o(mult_result[i])
			);
        end
    endgenerate

    // 累算器 - 第 1 段（加算 4 回ごと）
	integer k;
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            for (k = 0; k < 8; k = k + 1) begin
                accum_result_stage1[k] <= 16'h0;
            end
        end else begin
            for (k = 0; k < 8; k = k + 1) begin
                accum_result_stage1[k] <= mult_result[4*k] + mult_result[4*k+1] + mult_result[4*k+2] + mult_result[4*k+3];
            end
        end
    end

    // 累算器 - 第 2 段（加算 2 回ごと）
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            for (k = 0; k < 2; k = k + 1) begin
                accum_result_stage2[k] <= 16'h0;
            end
        end else begin
            for (k = 0; k < 2; k = k + 1) begin
                accum_result_stage2[k] <= accum_result_stage1[4*k] + accum_result_stage1[4*k+1] + accum_result_stage1[4*k+2] + accum_result_stage1[4*k+3];
            end
        end
    end

    always @(posedge clk or posedge rst) begin
        if (rst) begin
			mult_result_d1 <= 16'd0;
    		mult_result_d2 <= 16'd0;
        end else begin
			mult_result_d1 <= mult_result[32];
    		mult_result_d2 <= mult_result_d1;
        end
    end



    // 累算器 - 最終段
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            final_result <= 16'h0;
        end else begin
            final_result <= accum_result_stage2[0] + accum_result_stage2[1] + mult_result_d2;
        end
    end

	assign data_out = final_result ;

    // 遅延線（入力データの保持用）
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            for (k = 0; k < 33; k = k + 1) begin
                data_buffer[k] <= 16'h0;
            end
        end else begin
            data_buffer[32] <= data_buffer[31];
            for (k = 31; k > 0; k = k - 1) begin
                data_buffer[k] <= data_buffer[k-1];
            end
            data_buffer[0] <= data_in;
        end
    end


//wire [15:0] accum_0 = accum_result_stage1 [0];
//wire [15:0] accum_1 = accum_result_stage1 [1];
//wire [15:0] accum_2 = accum_result_stage1 [2];
//wire [15:0] accum_3 = accum_result_stage1 [3];
//wire [15:0] accum_4 = accum_result_stage1 [4];
//wire [15:0] accum_5 = accum_result_stage1 [5];
//wire [15:0] accum_6 = accum_result_stage1 [6];
//wire [15:0] accum_7 = accum_result_stage1 [7];

//wire [15:0] accum_20 = accum_result_stage2 [0];
//wire [15:0] accum_21 = accum_result_stage2 [1];


assign testvec = {
	accum_result_stage2 [0][15:0] ,
	accum_result_stage1 [0],
	data_in[15:0],
	coeff[1][15:8],
	coeff[0][15:8]
};

endmodule

