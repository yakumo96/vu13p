/*

あなたは Linux アプリケーション設計の専門家で、Xilinx の PCIe/XDMA の Linux ユーザー空間ドライバ開発に精通しています。PCIe 接続の FPGA デバイスがあり、FPGA 内には AXI DMA モジュールと DDR メモリ領域があります。

AXI DMA のドライバを書いてください。このハードウェアデバイスには、キャラクタデバイス BAR_SPACE_FD のオフセット DMA_DEV_BASE_ADDR でアクセスします。
ディスクリプタは 4 個固定でリング状に構成し、S2MM_BUFFER_SIZE を 4 等分して 4 つのバッファを作り、IOC 割り込みを有効にしてください。
割り込み処理用に別スレッドを作成し、割り込み内で割り込みフラグをクリアして、各ディスクリプタの status&0x80000000 を順にポーリングしてください。ディスクリプタが完了していれば別のワークキューに投入し、C2H0_BUFFER_FD 経由で buffer の内容を読み出してファイルに書き込んでください


追加の要件：
1. グローバル変数は使わないこと。共有ライブラリにしたい;
2. AXI DMA コントローラの起動順序に注意：先に DMCSR_RS を設定し、その後 tail desc を設定しないと起動しない
3. open(/dev/xdma0_user) した後の mmap では、オフセットアドレス DMA_DESC_BASE_ADDR と device のオフセットアドレス DMA_DEV_BASE_ADDR を直接指定すること。この 2 つの空間にはポインタでアクセスできる
4. ディスクリプタを設定する際は、memset() をスマートに使うこと
8. axidma_s2mm.h/axidma_s2mm.c を適切に分け、さらにテスト用の recv_test.c、共有ライブラリを呼び出す recv_test_so.c、そして Makefile を書くこと



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



// DMA ステータスレジスタのビット定義
#define DMASR_DLY_IRQ  0x2000  // 遅延割り込み





#endif /* _AXIDMA_CFG_H_ */



