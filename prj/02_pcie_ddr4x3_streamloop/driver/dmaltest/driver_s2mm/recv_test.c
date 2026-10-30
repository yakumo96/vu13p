/**
 * @file recv_test.c
 * @brief AXI DMA S2MM 受信テストプログラム
 */

#include "axidma_s2mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// シグナル処理用のグローバルハンドル
static axidma_s2mm_t *g_handle = NULL;

// シグナルハンドラ
static void signal_handler(int sig)
{
    printf("Received signal %d, exiting...\n", sig);
    if (g_handle) {
        axidma_s2mm_stop(g_handle);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    const char *output_file = "received_data.bin";
    int run_time = 30;  // デフォルトで 30 秒実行
    
    // コマンドライン引数を解析
    if (argc > 1) {
        output_file = argv[1];
    }
    
    if (argc > 2) {
        run_time = atoi(argv[2]);
        if (run_time <= 0) {
            run_time = 30;
        }
    }
    
    printf("AXI DMA S2MM Receive Test\n");
    printf("Output file: %s\n", output_file);
    printf("Run time: %d seconds\n", run_time);
    
    // シグナル処理を設定
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // AXI DMA S2MM を初期化
    g_handle = axidma_s2mm_init(output_file);
    if (!g_handle) {
        fprintf(stderr, "Failed to initialize AXI DMA S2MM\n");
        return 1;
    }
    
    // AXI DMA S2MM 転送を開始
    if (axidma_s2mm_start(g_handle) != 0) {
        fprintf(stderr, "Failed to start AXI DMA S2MM\n");
        axidma_s2mm_free(g_handle);
        return 1;
    }
    
    printf("AXI DMA S2MM running, press Ctrl+C to stop...\n");
    
    // 指定時間だけ実行
    sleep(run_time);
    
    // AXI DMA S2MM 転送を停止
    axidma_s2mm_stop(g_handle);
    
    // リソースを解放
    axidma_s2mm_free(g_handle);
    g_handle = NULL;
    
    printf("AXI DMA S2MM test completed\n");
    
    return 0;
}

