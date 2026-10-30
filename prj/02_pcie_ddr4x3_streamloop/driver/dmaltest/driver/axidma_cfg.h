/* 
あなたは Linux アプリケーション設計の専門家で、Xilinx の PCIe/XDMA の Linux ユーザー空間ドライバ開発に精通しています。PCIe 接続の FPGA デバイスがあり、FPGA 内には AXI DMA モジュールと DDR メモリ領域があります。

1. mm2s ドライバを作成し、host 側ディスク上のファイルを pcie 経由で FPGA 内部の DDR に送ってバッファとし、その後 AXI DMA コントローラを設定してデータを送り出せばよい:
1.1 まずディスクリプタを設定する：ディスクリプタの個数は固定の MM2S_DESC_COUNT とし、ディスクリプタを循環リンクリストにつなぐ。各ディスクリプタは MM2S_ONE_PACKET_SIZE のデータを DMA でき、buffer 全体は MM2S_BUFFER_SIZE 固定とする      
1.2 初期化段階で /dev/xdma0_h2c_0 経由で buffer（ベースアドレスは MM2S_BUFFER_BASEADDR）を一度に満たす
1.3 送信するファイルは必ず MM2S_BUFFER_SIZE より小さいこと。そうでなければ非対応と表示し、buffer を大きくするよう提案する
1.3.1 ファイル size が 1 パケットのサイズ MM2S_ONE_PACKET_SIZE 以下の場合、正しい size を設定することに注意。DMA 起動時、単発再生なら開始／終了ディスクリプタを設定して 1 回再生すればよい；ユーザーが無限再生を設定した場合は cyclic BD Enable を有効にする
1.3.2 ファイル size が 1 パケットより大きく buffer 全体のサイズ以下の場合も、そのまま buffer を満たせばよい。DMA 起動時に開始／終了ディスクリプタの位置を設定する。ユーザーが無限再生を必要とする場合は cyclic BD Enable を有効にする
1.4 ユーザーが終了キーを押したら cyclic BD disable を設定し、つまり循環ディスクリプタを終了させて、DMA コントローラが buffer の内容を送り終えるのを待つ;すべてのデータが送信済みであることを確認してから DMA を停止する

追加の要件：
1. グローバル変数は使わないこと。共有ライブラリにしたい;
2. AXI DMA コントローラの起動順序に注意：先に DMCSR_RS を設定し、その後 tail desc を設定しないと起動しない
3. open(/dev/xdma0_user) した後の mmap では、オフセットアドレス DMA_DESC_BASE_ADDR と device のオフセットアドレス DMA_DEV_BASE_ADDR を直接指定すること。この 2 つの空間にアクセスする際は、構造体ポインタでスマートに指し示すこと
4. ディスクリプタを設定する際は、memset() をスマートに使うこと
6. /dev/xdma0_h2c_0 へのアクセスは write 操作のみで行うこと
7. さらに、ファイルを読む代わりにメモリ上のデータストリームを転送できるよう、ユーザーが共有ライブラリ経由で呼び出せる API 関数を別途作成すること。
8. axidma_mm2s.h/axidma_mm2s.c を適切に分け、さらにテスト用の play_test.c、共有ライブラリを呼び出す play_test_so.c、そして Makefile を書くこと

*/


#ifndef _AXIDMA_CFG_H_
#define _AXIDMA_CFG_H_

// デバイスファイルのパス
#define BAR_SPACE_FD    "/dev/xdma0_user"
#define C2H0_BUFFER_FD  "/dev/xdma0_c2h_0"
#define H2C0_BUFFER_FD  "/dev/xdma0_h2c_0"
#define MM2S_INTERP_FD  "/dev/xdma0_events_2"
#define S2MM_INTERP_FD  "/dev/xdma0_events_3"

// 物理アドレスの定義
#define DMA_DESC_BASE_ADDR 0x30000  // DMA ディスクリプタの物理ベースアドレス
#define DMA_DESC_ALL_SIZE  0x10000  // ディスクリプタ格納領域のサイズ
#define DMA_DEV_BASE_ADDR  0x50000  // AXI DMA デバイスの物理アドレス、サイズは 64k 固定

// バッファと転送サイズの定義
#define MM2S_BUFFER_BASEADDR 0x000000000
#define MM2S_BUFFER_SIZE      (4ULL * 1024 * 1024 * 1024)  // 4GB
#define MM2S_DESC_COUNT       (DMA_DESC_ALL_SIZE/64/2  )   // 最大ディスクリプタ数。64 は 1 ディスクリプタのサイズ、2 は S2MM/MM2S で半分ずつ使うため
#define MM2S_ONE_PACKET_SIZE  (MM2S_BUFFER_SIZE/MM2S_DESC_COUNT)


// バッファと転送サイズの定義
#define S2MM_BUFFER_BASEADDR 0x100000000        // 
#define S2MM_BUFFER_SIZE     (128 * 1024 * 1024)  // 128MB
#define S2MM_WORK_QUEUE      32                   // DMA からホスト PC へのワークキューのサイズ







#endif /* _AXIDMA_CFG_H_ */



