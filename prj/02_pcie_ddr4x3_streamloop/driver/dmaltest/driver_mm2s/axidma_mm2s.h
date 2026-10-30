/**
 * @file axidma_mm2s.h
 * @brief AXI DMA MM2S ドライバのヘッダファイル
 */

#ifndef _AXIDMA_MM2S_H_
#define _AXIDMA_MM2S_H_

#include <stdint.h>
#include <stdbool.h>
#include "axidma_cfg.h"

/**
 * @brief AXI DMA MM2S ディスクリプタ構造体
 */
typedef struct {
    uint32_t nxtdesc;           // 次のディスクリプタポインタ
    uint32_t nxtdesc_msb;       // 次のディスクリプタポインタの上位 32 ビット
    uint32_t buffer_addr;       // バッファアドレス
    uint32_t buffer_addr_msb;   // バッファアドレスの上位 32 ビット
    uint32_t reserved1;         // 予約
    uint32_t reserved2;         // 予約
    uint32_t control;           // 制御ワード
    uint32_t status;            // ステータスワード
    uint32_t app0;              // アプリケーション固有データ
    uint32_t app1;              // アプリケーション固有データ
    uint32_t app2;              // アプリケーション固有データ
    uint32_t app3;              // アプリケーション固有データ
    uint32_t app4;              // アプリケーション固有データ
    uint32_t app5;              // アプリケーション固有データ
    uint32_t app6;              // アプリケーション固有データ
    uint32_t app7;              // アプリケーション固有データ
} mm2s_descriptor_t;

/**
 * @brief AXI DMA MM2S コンテキスト構造体
 */
typedef struct {
    int user_fd;                // /dev/xdma0_user のファイルディスクリプタ
    int h2c_fd;                 // /dev/xdma0_h2c_0 のファイルディスクリプタ
    int intr_fd;                // /dev/xdma0_events_2 のファイルディスクリプタ
    
    void *dma_desc_vaddr;       // DMA ディスクリプタの仮想アドレス
    void *dma_dev_vaddr;        // DMA デバイスレジスタの仮想アドレス
    
    volatile mm2s_descriptor_t *desc;    // ディスクリプタ配列へのポインタ
    uint64_t buffer_size;       // バッファサイズ
    uint64_t buffer_addr;       // バッファの物理アドレス
    
    bool is_cyclic;             // ループ再生するかどうか
    bool is_running;            // 実行中かどうか
    bool file_completed;        // ファイルの読み込みが完了したかどうか
    
    uint64_t file_size;         // ファイルサイズ
    int file_fd;                // ファイルディスクリプタ
    
    uint64_t current_offset;    // 現在のファイルオフセット
    int current_desc_idx;       // 現在のディスクリプタのインデックス
} axidma_mm2s_t;

/**
 * @brief AXI DMA MM2S ドライバを初期化
 * 
 * @param is_cyclic ループ再生するかどうか
 * @return axidma_mm2s_t* 成功時はコンテキストポインタ、失敗時は NULL を返す
 */
axidma_mm2s_t* axidma_mm2s_init(bool is_cyclic);

/**
 * @brief AXI DMA MM2S ドライバのリソースを解放
 * 
 * @param ctx コンテキストポインタ
 */
void axidma_mm2s_free(axidma_mm2s_t *ctx);

/**
 * @brief ファイルからデータを再生（送出）
 * 
 * @param ctx コンテキストポインタ
 * @param filename ファイル名
 * @return int 成功時は 0、失敗時は負の値を返す
 */
int axidma_mm2s_play_file(axidma_mm2s_t *ctx, const char *filename);

/**
 * @brief メモリからデータを再生（送出）
 * 
 * @param ctx コンテキストポインタ
 * @param buffer メモリバッファ
 * @param size バッファサイズ
 * @return int 成功時は 0、失敗時は負の値を返す
 */
int axidma_mm2s_play_memory(axidma_mm2s_t *ctx, const void *buffer, uint64_t size);

/**
 * @brief 再生を停止
 * 
 * @param ctx コンテキストポインタ
 * @return int 成功時は 0、失敗時は負の値を返す
 */
int axidma_mm2s_stop(axidma_mm2s_t *ctx);

/**
 * @brief 再生の完了を待つ
 * 
 * @param ctx コンテキストポインタ
 * @return int 成功時は 0、失敗時は負の値を返す
 */
int axidma_mm2s_wait(axidma_mm2s_t *ctx);

#endif /* _AXIDMA_MM2S_H_ */

