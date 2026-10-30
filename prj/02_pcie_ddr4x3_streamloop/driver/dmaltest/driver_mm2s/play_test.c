/**
 * @file play_test.c
 * @brief AXI DMA MM2S ドライバのテストプログラム
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <stdint.h>
#include "axidma_mm2s.h"

// シグナル処理用のグローバル変数
static axidma_mm2s_t *g_ctx = NULL;

// シグナルハンドラ
static void signal_handler(int sig)
{
    if (g_ctx) {
        printf("\nReceived signal %d, stopping DMA transfer...\n", sig);
        axidma_mm2s_stop(g_ctx);
    }
    exit(0);
}

// キー入力検出のため端末を非カノニカルモードに設定
static void setup_terminal(void)
{
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &new_termios);
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

// 端末設定を復元
static void restore_terminal(void)
{
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &new_termios);
    new_termios.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <filename> [cyclic]\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    bool is_cyclic = (argc == 3 && strcmp(argv[2], "cyclic") == 0);
    
    // シグナルハンドラを登録
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 端末を非カノニカルモードに設定
    setup_terminal();
    
    printf("Initializing AXI DMA MM2S driver...\n");
    g_ctx = axidma_mm2s_init(is_cyclic);
    if (!g_ctx) {
        fprintf(stderr, "Failed to initialize AXI DMA MM2S driver\n");
        restore_terminal();
        return 1;
    }
    
    printf("Playing file: %s (Cyclic mode: %s)\n", filename, is_cyclic ? "enabled" : "disabled");
    int ret = axidma_mm2s_play_file(g_ctx, filename);
    if (ret != 0) {
        fprintf(stderr, "Failed to play file: %s\n", filename);
        axidma_mm2s_free(g_ctx);
        restore_terminal();
        return 1;
    }
    
    printf("DMA transfer started. Press any key to stop...\n");
    
    // DMA 転送の完了を待つ
    axidma_mm2s_wait(g_ctx);
    
    printf("DMA transfer completed\n");
    
    // リソースを解放
    axidma_mm2s_free(g_ctx);
    g_ctx = NULL;
    
    // 端末設定を復元
    restore_terminal();
    
    return 0;
}

