/**
 * @file axidma_s2mm.c
 * @brief AXI DMA S2MM ドライバの実装
 */

#include "axidma_s2mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

// ワークキューの構造体
typedef struct {
    axidma_s2mm_t *handle;
    uint32_t desc_idx;
} work_item_t;

// static 関数の宣言
static void* intr_handler_thread(void *arg);
static void process_completed_buffer(axidma_s2mm_t *handle, uint32_t desc_idx);

/**
 * @brief AXI DMA S2MM を初期化
 */
axidma_s2mm_t* axidma_s2mm_init(const char *output_file)
{
    axidma_s2mm_t *handle = NULL;
    
    // ハンドル用メモリを確保
    handle = (axidma_s2mm_t *)malloc(sizeof(axidma_s2mm_t));
    if (!handle) {
        perror("Failed to allocate handle memory");
        return NULL;
    }
    
    // ハンドルを初期化
    memset(handle, 0, sizeof(axidma_s2mm_t));
    
    // ミューテックスを初期化
    if (pthread_mutex_init(&handle->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(handle);
        return NULL;
    }
    
    // 出力ファイルのパスを保存
    if (output_file) {
        handle->output_file = strdup(output_file);
        if (!handle->output_file) {
            perror("Failed to allocate output file path");
            pthread_mutex_destroy(&handle->mutex);
            free(handle);
            return NULL;
        }
    }
    
    // BAR 空間のデバイスファイルを開く
    handle->bar_fd = open(BAR_SPACE_FD, O_RDWR);
    if (handle->bar_fd < 0) {
        perror("Failed to open BAR space device");
        if (handle->output_file) free(handle->output_file);
        pthread_mutex_destroy(&handle->mutex);
        free(handle);
        return NULL;
    }
    
    // C2H バッファのデバイスファイルを開く
    handle->c2h_fd = open(C2H0_BUFFER_FD, O_RDWR);
    if (handle->c2h_fd < 0) {
        perror("Failed to open C2H buffer device");
        close(handle->bar_fd);
        if (handle->output_file) free(handle->output_file);
        pthread_mutex_destroy(&handle->mutex);
        free(handle);
        return NULL;
    }
    
    // 割り込みデバイスファイルを開く
    handle->intr_fd = open(S2MM_INTERP_FD, O_RDWR);
    if (handle->intr_fd < 0) {
        perror("Failed to open interrupt device");
        close(handle->c2h_fd);
        close(handle->bar_fd);
        if (handle->output_file) free(handle->output_file);
        pthread_mutex_destroy(&handle->mutex);
        free(handle);
        return NULL;
    }
    
    // DMA レジスタをマップ
    handle->dma_regs = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, 
                           handle->bar_fd, DMA_DEV_BASE_ADDR);
    if (handle->dma_regs == MAP_FAILED) {
        perror("Failed to map DMA registers");
        close(handle->intr_fd);
        close(handle->c2h_fd);
        close(handle->bar_fd);
        if (handle->output_file) free(handle->output_file);
        pthread_mutex_destroy(&handle->mutex);
        free(handle);
        return NULL;
    }
    
    // ディスクリプタ空間をマップ
    handle->desc_vaddr = mmap(NULL, DMA_DESC_ALL_SIZE/2, PROT_READ | PROT_WRITE, 
                             MAP_SHARED, handle->bar_fd, DMA_DESC_BASE_ADDR+ DMA_DESC_ALL_SIZE/2);
    if (handle->desc_vaddr == MAP_FAILED) {
        perror("Failed to map descriptor space");
        munmap(handle->dma_regs, 0x10000);
        close(handle->intr_fd);
        close(handle->c2h_fd);
        close(handle->bar_fd);
        if (handle->output_file) free(handle->output_file);
        pthread_mutex_destroy(&handle->mutex);
        free(handle);
        return NULL;
    }
    
    // バッファのパラメータを設定
    handle->num_buffers = 4;  // バッファは 4 個固定
    handle->buffer_size = S2MM_BUFFER_SIZE / handle->num_buffers;
    
    printf("AXI DMA S2MM initialized successfully\n");
    printf("Buffer size: %u bytes, Number of buffers: %u\n", 
           handle->buffer_size, handle->num_buffers);
    
    return handle;
}

/**
 * @brief S2MM ディスクリプタを設定
 */
static int configure_s2mm_descriptors(axidma_s2mm_t *handle)
{
    axidma_desc_t *desc_base = (axidma_desc_t *)handle->desc_vaddr;
    uint32_t desc_phys_addr = DMA_DESC_BASE_ADDR+DMA_DESC_ALL_SIZE/2;
    uint64_t buffer_addr = S2MM_BUFFER_BASEADDR;
    
    // 4 個のディスクリプタを設定
    for (uint32_t i = 0; i < handle->num_buffers; i++) {
        axidma_desc_t *desc = &desc_base[i];
        uint32_t next_idx = (i + 1) % handle->num_buffers;
        uint32_t next_desc_phys = desc_phys_addr + (next_idx * 0x40);
        
        // ディスクリプタをクリア
        memset(desc, 0, sizeof(axidma_desc_t));
        
        // 次のディスクリプタアドレスを設定
        desc->nxtdesc = next_desc_phys;
        desc->nxtdesc_msb = 0;  // 64 ビットアドレスは使用しない前提
        
        // バッファアドレスを設定
        desc->buffer_addr = (buffer_addr + (i * handle->buffer_size) ) & 0xFFFFFFFF;
        desc->buffer_addr_msb = (buffer_addr >> 32);
        
        // バッファサイズを含む制御ワードを設定
        desc->control = handle->buffer_size;
        
        // ステータスワードを 0 に初期化
        desc->status = 0;
        
        printf("Descriptor %u: next=0x%08x, buffer=0x%08x%08x, size=0x%08x\n",
               i, desc->nxtdesc, desc->buffer_addr_msb, desc->buffer_addr, handle->buffer_size);
    }
    
    return 0;
}

/**
 * @brief AXI DMA S2MM 転送を開始
 */
int axidma_s2mm_start(axidma_s2mm_t *handle)
{
    volatile uint32_t *s2mm_dmacr;
    volatile uint32_t *s2mm_dmasr;
    volatile uint32_t *s2mm_curdesc;
    volatile uint32_t *s2mm_taildesc;
    volatile uint32_t *s2mm_taildesc_msb;
    uint32_t reg_val;
    int ret;
    
    if (!handle) {
        return -EINVAL;
    }
    
    // レジスタアドレスを取得
    s2mm_dmacr = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_DMACR);
    s2mm_dmasr = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_DMASR);
    s2mm_curdesc = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_CURDESC);
    s2mm_taildesc = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_TAILDESC);
    s2mm_taildesc_msb = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_TAILDESC_MSB);
    
    // ディスクリプタを設定
    ret = configure_s2mm_descriptors(handle);
    if (ret != 0) {
        return ret;
    }
    
    // DMA コントローラをリセット
	//*s2mm_dmacr = DMACR_RESET;
    //usleep(10000);  // リセット完了を待つ
    *s2mm_dmacr = 0;
    
    // DMA の状態を確認
    reg_val = *s2mm_dmasr;
    if (!(reg_val & DMASR_HALTED)) {
        fprintf(stderr, "DMA not halted after reset: 0x%08x\n", reg_val);
        return -EIO;
    }
    
    // カレントディスクリプタアドレスを設定
    *s2mm_curdesc = DMA_DESC_BASE_ADDR+DMA_DESC_ALL_SIZE/2;
    
    // 割り込み処理スレッドを起動
    handle->thread_running = true;
    if (pthread_create(&handle->intr_thread, NULL, intr_handler_thread, handle) != 0) {
        perror("Failed to create interrupt handler thread");
        return -EFAULT;
    }
    
    // DMA 制御レジスタを設定し、IOC 割り込みを有効化
    *s2mm_dmacr = DMACR_RS | DMACR_IOC_IRQ | DMACR_CYCLIC;
    
    // テールディスクリプタアドレスを設定して DMA を起動
    *s2mm_taildesc = (DMA_DESC_ALL_SIZE/2 + DMA_DESC_BASE_ADDR) + ((handle->num_buffers - 1) * 0x40);
	*s2mm_taildesc_msb = 0;
    
    printf("AXI DMA S2MM started\n");
    printf("DMACR: 0x%08x, DMASR: 0x%08x\n", *s2mm_dmacr, *s2mm_dmasr);
    printf("CURDESC: 0x%08x, TAILDESC: 0x%08x\n", *s2mm_curdesc, *s2mm_taildesc);
    
    return 0;
}

/**
 * @brief AXI DMA S2MM 転送を停止
 */
int axidma_s2mm_stop(axidma_s2mm_t *handle)
{
    volatile uint32_t *s2mm_dmacr;
    
    if (!handle) {
        return -EINVAL;
    }
    
    // レジスタアドレスを取得
    s2mm_dmacr = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_DMACR);
    
    // DMA を停止
    *s2mm_dmacr = 0;  // RS ビットをクリア
    
    // 割り込み処理スレッドを停止
    if (handle->thread_running) {
        handle->thread_running = false;
        pthread_join(handle->intr_thread, NULL);
    }
    
    printf("AXI DMA S2MM stopped\n");
    
    return 0;
}

/**
 * @brief AXI DMA S2MM のリソースを解放
 */
void axidma_s2mm_free(axidma_s2mm_t *handle)
{
    if (!handle) {
        return;
    }
    
    // DMA を停止
    axidma_s2mm_stop(handle);
    
    // メモリマップを解除
    if (handle->desc_vaddr != MAP_FAILED && handle->desc_vaddr != NULL) {
        munmap(handle->desc_vaddr, DMA_DESC_ALL_SIZE/2);
    }
    
    if (handle->dma_regs != MAP_FAILED && handle->dma_regs != NULL) {
        munmap(handle->dma_regs, 0x10000);
    }
    
    // ファイルディスクリプタを閉じる
    if (handle->intr_fd >= 0) {
        close(handle->intr_fd);
    }
    
    if (handle->c2h_fd >= 0) {
        close(handle->c2h_fd);
    }
    
    if (handle->bar_fd >= 0) {
        close(handle->bar_fd);
    }
    
    // 出力ファイルのパスを解放
    if (handle->output_file) {
        free(handle->output_file);
    }
    
    // ミューテックスを破棄
    pthread_mutex_destroy(&handle->mutex);
    
    // ハンドルを解放
    free(handle);
    
    printf("AXI DMA S2MM resources freed\n");
}

/**
 * @brief 割り込み処理スレッド
 */
static void* intr_handler_thread(void *arg)
{
    axidma_s2mm_t *handle = (axidma_s2mm_t *)arg;
    volatile uint32_t *s2mm_dmasr;
    axidma_desc_t *desc_base;
	uint64_t intpcnt=0;
    uint32_t events;
    volatile uint32_t *s2mm_dmacr;
    volatile uint32_t *s2mm_taildesc;
    volatile uint32_t *s2mm_taildesc_msb;
    s2mm_dmacr = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_DMACR);
    s2mm_taildesc = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_TAILDESC);
    s2mm_taildesc_msb = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_TAILDESC_MSB);
    
    // レジスタアドレスを取得
    s2mm_dmasr = (volatile uint32_t *)((char *)handle->dma_regs + S2MM_DMASR);
    desc_base = (axidma_desc_t *)handle->desc_vaddr;
    
    printf("Interrupt handler thread started\n");
    
    while (handle->thread_running) {
        // 割り込みを待つ
        if (read(handle->intr_fd, &events, sizeof(events)) < 0) {
            if (errno == EINTR) {
                continue;  // シグナルで中断された。待機を続ける
            }
            perror("Failed to read interrupt event");
            break;
        }
        //if (desc_base[0].status & 0x80000000) {
			*s2mm_dmacr = DMACR_RS | DMACR_IOC_IRQ | DMACR_CYCLIC;
			*s2mm_taildesc = (DMA_DESC_ALL_SIZE/2 + DMA_DESC_BASE_ADDR) + ((handle->num_buffers - 1) * 0x40);
			*s2mm_taildesc_msb = 0;
		//}
        
        // 割り込みの種類を確認
        uint32_t status = *s2mm_dmasr;
        
        // 割り込みフラグをクリア
        if (status & DMASR_IOC_IRQ) {
            *s2mm_dmasr = status | DMASR_IOC_IRQ;
			printf("IOC interrupt %03ld received, status: 0x%08x\n",intpcnt++, status);
        }
        
        //// エラーを確認
        //if (status & DMASR_ERR_IRQ) {
        //    *s2mm_dmasr = status | DMASR_ERR_IRQ;
        //    fprintf(stderr, "DMA error interrupt received: 0x%08x\n", status);
        //}
        
        // すべてのディスクリプタの状態をポーリング
        for (uint32_t i = 0; i < handle->num_buffers; i++) {
            // ディスクリプタの完了フラグを確認 (STATUS_COMPLETED = 0x80000000)
            if (desc_base[i].status & 0x80000000) {
                // 完了したバッファを処理
                process_completed_buffer(handle, i);
                
                // 完了フラグをクリア
                desc_base[i].status &= ~0x80000000;
            }
        }
    }
    
    printf("Interrupt handler thread exited\n");
    return NULL;
}

/**
 * @brief 完了したバッファを処理
 */
static void process_completed_buffer(axidma_s2mm_t *handle, uint32_t desc_idx)
{
    axidma_desc_t *desc = &((axidma_desc_t *)handle->desc_vaddr)[desc_idx];
    uint32_t bytes_received = desc->status & 0x03FFFFFF;  // 下位 26 ビットが転送バイト数
    uint32_t buffer_offset = desc->buffer_addr - S2MM_BUFFER_BASEADDR;
    
    //printf("Buffer %u completed: %u bytes received\n", desc_idx, bytes_received);
    
    // データを受信していなければスキップ
    if (bytes_received == 0) {
        return;
    }
    
    // 出力ファイルが指定されていなければスキップ
    if (!handle->output_file) {
        return;
    }
    
    // 受信したデータを読み出す
    void *buffer = malloc(bytes_received);
    if (!buffer) {
        perror("Failed to allocate buffer memory");
        return;
    }
    
    // C2H デバイスからデータを読み出す
    if (pread(handle->c2h_fd, buffer, bytes_received, buffer_offset) != bytes_received) {
        perror("Failed to read data from C2H buffer");
        free(buffer);
        return;
    }
    
    // 出力ファイルに書き込む
    FILE *fp = fopen(handle->output_file, "ab");
    if (!fp) {
        perror("Failed to open output file");
        free(buffer);
        return;
    }
    
    // データを書き込む
    if (fwrite(buffer, 1, bytes_received, fp) != bytes_received) {
        perror("Failed to write data to output file");
    }
    
    fclose(fp);
    free(buffer);
}

