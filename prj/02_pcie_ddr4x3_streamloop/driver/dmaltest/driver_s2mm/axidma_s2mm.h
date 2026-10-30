/**
 * @file axidma_s2mm.h
 * @brief AXI DMA S2MM ドライバのヘッダファイル
 */

#ifndef _AXIDMA_S2MM_H_
#define _AXIDMA_S2MM_H_

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

// axidma_cfg.h から設定を読み込む
#include "axidma_cfg.h"

/**
 * @brief AXI DMA S2MM ディスクリプタ構造体
 */
typedef struct {
    uint32_t nxtdesc;           // 次のディスクリプタアドレス
    uint32_t nxtdesc_msb;       // 次のディスクリプタアドレスの上位 32 ビット
    uint32_t buffer_addr;       // バッファアドレス
    uint32_t buffer_addr_msb;   // バッファアドレスの上位 32 ビット
    uint32_t reserved1;         // 予約
    uint32_t reserved2;         // 予約
    uint32_t control;           // 制御ワード
    uint32_t status;            // ステータスワード
    uint32_t app[8];            // アプリケーション固有データ
} axidma_desc_t;

/**
 * @brief AXI DMA S2MM ハンドル構造体
 */
typedef struct {
    int bar_fd;                 // BAR 空間のファイルディスクリプタ
    int c2h_fd;                 // C2H バッファのファイルディスクリプタ
    int intr_fd;                // 割り込みのファイルディスクリプタ
    
    void *dma_regs;             // DMA レジスタのマッピング
    void *desc_vaddr;           // ディスクリプタの仮想アドレス
    
    uint32_t buffer_size;       // 個々のバッファのサイズ
    uint32_t num_buffers;       // バッファの数
    
    pthread_t intr_thread;      // 割り込み処理スレッド
    bool thread_running;        // スレッド実行フラグ
    
    char *output_file;          // 出力ファイルのパス
    
    pthread_mutex_t mutex;      // ミューテックス
} axidma_s2mm_t;

/**
 * @brief AXI DMA S2MM を初期化
 * 
 * @param output_file 出力ファイルのパス
 * @return axidma_s2mm_t* 成功時はハンドル、失敗時は NULL を返す
 */
axidma_s2mm_t* axidma_s2mm_init(const char *output_file);

/**
 * @brief AXI DMA S2MM 転送を開始
 * 
 * @param handle AXI DMA S2MM ハンドル
 * @return int 成功時は 0、失敗時は負の値を返す
 */
int axidma_s2mm_start(axidma_s2mm_t *handle);

/**
 * @brief AXI DMA S2MM 転送を停止
 * 
 * @param handle AXI DMA S2MM ハンドル
 * @return int 成功時は 0、失敗時は負の値を返す
 */
int axidma_s2mm_stop(axidma_s2mm_t *handle);

/**
 * @brief AXI DMA S2MM のリソースを解放
 * 
 * @param handle AXI DMA S2MM ハンドル
 */
void axidma_s2mm_free(axidma_s2mm_t *handle);

#endif /* _AXIDMA_S2MM_H_ */

