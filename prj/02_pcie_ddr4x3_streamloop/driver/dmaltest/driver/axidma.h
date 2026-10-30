#ifndef _AXIDMA_H_
#define _AXIDMA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <sys/poll.h>   // poll 関数と関連定数のため
typedef struct {
    int dma_fd;             // DMA コントローラのファイルディスクリプタ
    int data_fd;            // データファイルのファイルディスクリプタ
    int event_fd;           // イベントのファイルディスクリプタ
    void *dma_regs;         // DMA レジスタのマッピング
    void *desc_base;        // ディスクリプタのベースアドレスのマッピング
    void *buffer;           // データバッファのマッピング
    uint64_t file_size;     // ファイルサイズ
    uint64_t bytes_sent;    // 送信済みバイト数
    uint64_t buffer_size;   // バッファサイズ
    uint32_t desc_count;         // ディスクリプタ数
    int is_cyclic;          // ループ（cyclic）モードかどうか
    pthread_t event_thread; // イベント処理スレッド
    int running;            // 実行フラグ
} mm2s_dma_t;


// ワークキュー項目の構造体
typedef struct {
    void *buffer;           // データバッファへのポインタ
    size_t size;            // データサイズ
    int frame_index;        // フレームインデックス
    struct timespec ts;     // タイムスタンプ
} work_item_t;

// ワークキューの構造体
typedef struct {
    work_item_t *items;     // ワーク項目の配列
    int capacity;           // キューの容量
    int head;               // キューの先頭
    int tail;               // キューの末尾
    int count;              // キュー内の項目数
    pthread_mutex_t mutex;  // ミューテックス
    pthread_cond_t not_empty; // 条件変数：キューが空でない
    pthread_cond_t not_full;  // 条件変数：キューが満杯でない
} work_queue_t;

// ディスクリプタの状態追跡
typedef struct {
    int processed;          // 処理済みかどうか
    uint32_t status;        // ディスクリプタの状態
    uint32_t bytes_received; // 受信したバイト数
    int is_sof;             // フレーム先頭かどうか
    int is_eof;             // フレーム末尾かどうか
} desc_status_t;

// S2MM DMA の構造体
typedef struct {
    int dma_fd;             // DMA コントローラのファイルディスクリプタ
    int data_fd;            // データファイルのファイルディスクリプタ
    int event_fd;           // イベントのファイルディスクリプタ
    void *dma_regs;         // DMA レジスタのマッピング
    void *desc_base;        // ディスクリプタのベースアドレスのマッピング
    void *buffer;           // データバッファのマッピング
    size_t buffer_size;     // バッファサイズ
    size_t desc_size;       // 各ディスクリプタのサイズ
    uint32_t desc_count;         // ディスクリプタ数
    size_t segment_size;    // 各ディスクリプタに対応するバッファのサイズ
    desc_status_t *desc_status; // ディスクリプタ状態の配列
    work_queue_t work_queue;  // ワークキュー
    pthread_t event_thread;   // イベント処理スレッド
    pthread_t worker_thread;  // ワーカースレッド
    int running;            // 実行フラグ
    int frame_count;        // 受信したフレーム数
    char output_dir[256];   // 出力ディレクトリ
} s2mm_dma_t;


// 関数宣言
int init_mm2s_dma(mm2s_dma_t *dma, const char *filename);
int load_file_to_buffer(mm2s_dma_t *dma, const char *filename);
int setup_mm2s_descriptors(mm2s_dma_t *dma);
int start_mm2s_dma(mm2s_dma_t *dma);
void *event_handler_thread(void *arg);




// 関数宣言
int init_s2mm_dma(s2mm_dma_t *dma, const char *output_dir);
int setup_s2mm_descriptors(s2mm_dma_t *dma);
int start_s2mm_dma(s2mm_dma_t *dma);
void cleanup_s2mm_dma(s2mm_dma_t *dma);
int init_work_queue(work_queue_t *queue, int capacity);
int enqueue_work(work_queue_t *queue, void *buffer, size_t size, int frame_index);
int dequeue_work(work_queue_t *queue, work_item_t *item);
void destroy_work_queue(work_queue_t *queue);
void *event_handler_thread(void *arg);
void *worker_thread(void *arg);




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










#endif /* _AXIDMA_H_ */
