/*

あなたは Linux アプリケーション設計の専門家で、Xilinx の PCIe/XDMA の Linux ユーザー空間ドライバ開発に精通しています。PCIe 接続の FPGA デバイスがあり、FPGA 内には AXI DMA モジュールと DDR メモリ領域があります。

1. mm2s ドライバを作成し、host 側ディスク上のファイルを pcie 経由で FPGA 内部の DDR に送ってバッファとし、その後 AXI DMA コントローラを設定してデータを送り出せばよい:
1.1 まずディスクリプタを設定する：ディスクリプタはピンポン動作用に 2 個固定で、2 つのディスクリプタで buffer サイズ MM2S_BUFFER_SIZE を 2 等分する
1.2 初期化段階で /dev/xdma0_h2c_0 経由で buffer（ベースアドレスは MM2S_BUFFER_BASEADDR）を一度に満たす
1.3 ユーザーが単発再生を設定した場合、ファイルを送り終えたら終了する。ファイルサイズに応じて場合分けする：
1.3.1 ファイルが MM2S_BUFFER_SIZE/2 以下なら、内容を複製して 2 つの buffer に書き込む，
1.3.2 そうでなくファイルが MM2S_BUFFER_SIZE 以下なら、前半 MM2S_BUFFER_SIZE を 1 つ目の buffer に、後半 MM2S_BUFFER_SIZE を 2 つ目の buffer に書き込む;
1.3.3 それ以外（ファイルが buffer size より大きい）の場合、前半 MM2S_BUFFER_SIZE を 1 つ目の buffer に、後半 MM2S_BUFFER_SIZE を 2 つ目の buffer に書き込む; 残りの内容は割り込み内でピンポン方式で埋める。ここでは cyclic BD Enable を設定し、ファイルを読み終えたら cyclic BD disable を設定する

1.4 ユーザーが無限再生を設定した場合、ファイルを送り終えたら自動的に先頭に戻って送り続ける。ファイルサイズに応じて場合分けする：無限再生モードでは cyclic BD Enable を固定で設定する
1.4.1 ファイルが MM2S_BUFFER_SIZE/2 以下なら、内容を複製して 2 つの buffer に書き込む，
1.4.2 そうでなくファイルが MM2S_BUFFER_SIZE 以下なら、前半 MM2S_BUFFER_SIZE を 1 つ目の buffer に、後半 MM2S_BUFFER_SIZE を 2 つ目の buffer に書き込む;
1.4.3 それ以外（ファイルが buffer size より大きい）の場合、前半 MM2S_BUFFER_SIZE を 1 つ目の buffer に、後半 MM2S_BUFFER_SIZE を 2 つ目の buffer に書き込む; 残りの内容は割り込み内でピンポン方式で埋める


1.5 ディスク上のファイルサイズは任意で、123Byte/1kB/1MB/1GB/10GB/100GB、あるいは無限大（ループ再生）の可能性がある

1.6 割り込み内：単発再生の場合、ファイルを読み終えたら割り込みスレッドを終了する
1.7 割り込み内：無限再生の場合、ユーザーが任意のキーを押したら終了する




追加の要件：
1. グローバル変数は使わないこと。共有ライブラリにしたい;
2. AXI DMA コントローラの起動順序に注意：先に DMCSR_RS を設定し、その後 tail desc を設定しないと起動しない
3. open(/dev/xdma0_user) した後の mmap では、オフセットアドレス DMA_DESC_BASE_ADDR と device のオフセットアドレス DMA_DEV_BASE_ADDR を直接指定すること。この 2 つの空間にはポインタでアクセスできる
4. ディスクリプタを設定する際は、memset() をスマートに使うこと
6. /dev/xdma0_h2c_0 へのアクセスは write 操作のみで行うこと
7. さらに、ファイルを読む代わりにメモリ上のデータストリームを転送できるよう、ユーザーが共有ライブラリ経由で呼び出せる API 関数を別途作成すること。
8. axidma_mm2s.h/axidma_mm2s.c を適切に分け、さらにテスト用の play_test.c、共有ライブラリを呼び出す play_test_so.c、そして Makefile を書くこと


ハマりどころ：
1. XILINX の AXIDMA はストリームモードを一切サポートしておらず、パケットモードのみサポートしている。無理にバックツーバックでパケットを送ろうとすると、AXI-DMA には落とし穴が非常に多い。
2. まず BD Cyclic Enable の意味は、ディスクリプタを循環させてリンクリストを構成することではなく、ディスクリプタ内の完了信号を無視させて、ディスクリプタを繰り返し使えるようにすることである。
3. 複数のディスクリプタで循環チェーンを構成する場合は、割り込み内でディスクリプタを再起動する必要がある
4. xilinx の AXI DMA はパケットモードにしか使えない！！！！！

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
#define MM2S_BUFFER_SIZE     0x100000000


// バッファと転送サイズの定義
#define S2MM_BUFFER_BASEADDR 0x100000000        // 
#define S2MM_BUFFER_SIZE     (128 * 1024 * 1024)  // 128MB








// =====================================================================================
// =====================================================================================
// =====================================================================================
// 以下は固定のレジスタ
// DMA レジスタのオフセット
#define MM2S_DMACR        0x00// MM2S DMA Control Register
#define MM2S_DMASR        0x04// MM2S DMA Status Register
#define MM2S_CURDESC      0x08// MM2S Current Descriptor
#define MM2S_CURDESC_MSB  0x0C// MM2S Current Descriptor MSB
#define MM2S_TAILDESC     0x10// MM2S Tail Descriptor
#define MM2S_TAILDESC_MSB 0x14// MM2S Tail Descriptor MSB
							  // ==========================
#define S2MM_DMACR        0x30// S2MM DMA Control Register
#define S2MM_DMASR        0x34// S2MM DMA Status Register
#define S2MM_CURDESC      0x38// S2MM Current Descriptor
#define S2MM_CURDESC_MSB  0x3C// S2MM Current Descriptor MSB
#define S2MM_TAILDESC     0x40// S2MM Tail Descriptor
#define S2MM_TAILDESC_MSB 0x44// S2MM Tail Descriptor MSB

// ディスクリプタのオフセット
#define NXTDESC_OFFSET         0x00
#define NXTDESC_MSB_OFFSET     0x04
#define BUFFER_ADDR_OFFSET     0x08
#define BUFFER_ADDR_MSB_OFFSET 0x0C
#define CONTROL_OFFSET         0x18
#define STATUS_OFFSET          0x1C
#define APP_OFFSET             0x20  // アプリケーション固有データのオフセット
#define DESC_SIZE              64    // ディスクリプタのサイズ（バイト）

// 制御レジスタのビット
#define DMACR_RS         (1 << 0)    // Run/Stop
#define DMACR_RESET      (1 << 2)    // Reset
#define DMACR_KEYHOLE    (1 << 3)    // Keyhole
#define DMACR_CYCLIC     (1 << 4)    // Cyclic BD Enable
#define DMACR_IOC_IRQ    (1 << 12)   // IOC 割り込みイネーブル
#define DMACR_DLY_IRQ    (1 << 13)   // DLY 割り込みイネーブル
#define DMACR_ERR_IRQ    (1 << 14)   // エラー割り込みイネーブル
                         
// ステータスレジスタのビット          
#define DMASR_HALTED     (1 << 0)    // DMA 停止済み
#define DMASR_IDLE       (1 << 1)    // DMA アイドル
#define DMASR_IOC_IRQ    (1 << 12)   // IOC 割り込み
#define DMASR_ERR_IRQ    (1 << 14)   // エラー割り込み
                         
// ディスクリプタ制御ビット          
#define CONTROL_RXEOF    (1 << 26)   // End of Frame
#define CONTROL_RXSOF    (1 << 27)   // Start of Frame

// ディスクリプタステータスビット
#define STATUS_COMPLETED (1 << 31)  // ディスクリプタ完了
#define STATUS_RXEOF     (1 << 26)  // EOF を受信
#define STATUS_RXSOF     (1 << 27)  // SOF を受信


// ディスクリプタ制御ビット
#define CONTROL_TXEOF  (1 << 26)   // End of Frame
#define CONTROL_TXSOF  (1 << 27)   // Start of Frame


#endif /* _AXIDMA_CFG_H_ */



