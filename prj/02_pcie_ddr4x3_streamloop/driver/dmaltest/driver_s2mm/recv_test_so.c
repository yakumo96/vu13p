/**
 * @file recv_test_so.c
 * @brief 共有ライブラリを使う AXI DMA S2MM 受信テストプログラム
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dlfcn.h>

// 関数ポインタ型の定義
typedef void* (*init_func_t)(const char*);
typedef int (*start_func_t)(void*);
typedef int (*stop_func_t)(void*);
typedef void (*free_func_t)(void*);

// グローバル変数
static void *g_handle = NULL;
static void *g_lib_handle = NULL;
static stop_func_t g_stop_func = NULL;
static free_func_t g_free_func = NULL;

// シグナルハンドラ
static void signal_handler(int sig)
{
    printf("Received signal %d, exiting...\n", sig);
    if (g_handle && g_stop_func) {
        g_stop_func(g_handle);
    }
    
    if (g_handle && g_free_func) {
        g_free_func(g_handle);
    }
    
    if (g_lib_handle) {
        dlclose(g_lib_handle);
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
    
    printf("AXI DMA S2MM Receive Test (Dynamic Library)\n");
    printf("Output file: %s\n", output_file);
    printf("Run time: %d seconds\n", run_time);
    
    // シグナル処理を設定
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 共有ライブラリをロード
    g_lib_handle = dlopen("./libaxidma_s2mm.so", RTLD_LAZY);
    if (!g_lib_handle) {
        fprintf(stderr, "Failed to load libaxidma_s2mm.so: %s\n", dlerror());
        return 1;
    }
    
    // 関数ポインタを取得
    init_func_t init_func = (init_func_t)dlsym(g_lib_handle, "axidma_s2mm_init");
    g_stop_func = (stop_func_t)dlsym(g_lib_handle, "axidma_s2mm_stop");
    g_free_func = (free_func_t)dlsym(g_lib_handle, "axidma_s2mm_free");
    start_func_t start_func = (start_func_t)dlsym(g_lib_handle, "axidma_s2mm_start");
    
    if (!init_func || !g_stop_func || !g_free_func || !start_func) {
        fprintf(stderr, "Failed to get function pointers: %s\n", dlerror());
        dlclose(g_lib_handle);
        return 1;
    }
    
    // AXI DMA S2MM を初期化
    g_handle = init_func(output_file);
    if (!g_handle) {
        fprintf(stderr, "Failed to initialize AXI DMA S2MM\n");
        dlclose(g_lib_handle);
        return 1;
    }
    
    // AXI DMA S2MM 転送を開始
    if (start_func(g_handle) != 0) {
        fprintf(stderr, "Failed to start AXI DMA S2MM\n");
        g_free_func(g_handle);
        dlclose(g_lib_handle);
        return 1;
    }
    
    printf("AXI DMA S2MM running, press Ctrl+C to stop...\n");
    
    // 指定時間だけ実行
    sleep(run_time);
    
    // AXI DMA S2MM 転送を停止
    g_stop_func(g_handle);
    
    // リソースを解放
    g_free_func(g_handle);
    dlclose(g_lib_handle);
    
    printf("AXI DMA S2MM test completed\n");
    
    return 0;
}

