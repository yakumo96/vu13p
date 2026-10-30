/**
 * @file axidma_mm2s.c
 * @brief AXI DMA MM2S ドライバの実装
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <termios.h>
#include <stdint.h>
#include "axidma_mm2s.h"

// プライベート関数の宣言
static void* interrupt_handler(void *arg);
static int setup_descriptors(axidma_mm2s_t *ctx);
static int load_initial_data(axidma_mm2s_t *ctx);
static int start_dma_transfer(axidma_mm2s_t *ctx);
static int stop_dma_transfer(axidma_mm2s_t *ctx);
static int wait_for_keypress(void);

axidma_mm2s_t* axidma_mm2s_init(bool is_cyclic)
{
    axidma_mm2s_t *ctx = (axidma_mm2s_t *)malloc(sizeof(axidma_mm2s_t));
    if (!ctx) {
        perror("Failed to allocate memory for context");
        return NULL;
    }
    
    // コンテキストを初期化
    memset(ctx, 0, sizeof(axidma_mm2s_t));
    ctx->is_cyclic = is_cyclic;
    ctx->buffer_size = MM2S_BUFFER_SIZE/2 > 0x04000000 ? 0x04000000 : MM2S_BUFFER_SIZE/2 ;
    ctx->buffer_addr = MM2S_BUFFER_BASEADDR;
    
    // デバイスファイルを開く
    ctx->user_fd = open(BAR_SPACE_FD, O_RDWR);
    if (ctx->user_fd < 0) {
        perror("Failed to open BAR space device");
        free(ctx);
        return NULL;
    }
    
    ctx->h2c_fd = open(H2C0_BUFFER_FD, O_WRONLY);
    if (ctx->h2c_fd < 0) {
        perror("Failed to open H2C buffer device");
        close(ctx->user_fd);
        free(ctx);
        return NULL;
    }
    
    ctx->intr_fd = open(MM2S_INTERP_FD, O_RDONLY);
    if (ctx->intr_fd < 0) {
        perror("Failed to open MM2S interrupt device");
        close(ctx->h2c_fd);
        close(ctx->user_fd);
        free(ctx);
        return NULL;
    }
    
    // DMA ディスクリプタとデバイスレジスタをマップ
    ctx->dma_desc_vaddr = mmap(NULL, DMA_DESC_ALL_SIZE, PROT_READ | PROT_WRITE, 
                              MAP_SHARED, ctx->user_fd, DMA_DESC_BASE_ADDR);
    if (ctx->dma_desc_vaddr == MAP_FAILED) {
        perror("Failed to map DMA descriptor space");
        close(ctx->intr_fd);
        close(ctx->h2c_fd);
        close(ctx->user_fd);
        free(ctx);
        return NULL;
    }
    
    ctx->dma_dev_vaddr = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, 
                             MAP_SHARED, ctx->user_fd, DMA_DEV_BASE_ADDR);
    if (ctx->dma_dev_vaddr == MAP_FAILED) {
        perror("Failed to map DMA device space");
        munmap(ctx->dma_desc_vaddr, DMA_DESC_ALL_SIZE);
        close(ctx->intr_fd);
        close(ctx->h2c_fd);
        close(ctx->user_fd);
        free(ctx);
        return NULL;
    }
    
    // ディスクリプタポインタを設定
    ctx->desc = (mm2s_descriptor_t *)ctx->dma_desc_vaddr;
    
    // ディスクリプタを初期化
    if (setup_descriptors(ctx) != 0) {
        perror("Failed to setup descriptors");
        munmap(ctx->dma_dev_vaddr, 0x10000);
        munmap(ctx->dma_desc_vaddr, DMA_DESC_ALL_SIZE);
        close(ctx->intr_fd);
        close(ctx->h2c_fd);
        close(ctx->user_fd);
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void axidma_mm2s_free(axidma_mm2s_t *ctx)
{
    if (!ctx)
        return;
    
    // DMA 転送を停止
    if (ctx->is_running) {
        stop_dma_transfer(ctx);
    }
    
    // ファイルディスクリプタを閉じる
    if (ctx->file_fd > 0) {
        close(ctx->file_fd);
    }
    
    // メモリマップを解除
    munmap(ctx->dma_dev_vaddr, 0x10000);
    munmap(ctx->dma_desc_vaddr, DMA_DESC_ALL_SIZE);
    
    // デバイスファイルを閉じる
    close(ctx->intr_fd);
    close(ctx->h2c_fd);
    close(ctx->user_fd);
    
    // コンテキストを解放
    free(ctx);
}

int axidma_mm2s_play_file(axidma_mm2s_t *ctx, const char *filename)
{
    if (!ctx || !filename) {
        return -EINVAL;
    }
    
    // ファイルを開く
    ctx->file_fd = open(filename, O_RDONLY);
    if (ctx->file_fd < 0) {
        perror("Failed to open input file");
        return -errno;
    }
    
    // ファイルサイズを取得
    struct stat st;
    if (fstat(ctx->file_fd, &st) < 0) {
        perror("Failed to get file size");
        close(ctx->file_fd);
        ctx->file_fd = -1;
        return -errno;
    }
    
    ctx->file_size = st.st_size;
    ctx->current_offset = 0;
    ctx->file_completed = false;
    
    // 初期データをロード
    int ret = load_initial_data(ctx);
    if (ret != 0) {
        close(ctx->file_fd);
        ctx->file_fd = -1;
        return ret;
    }
    
    // DMA 転送を開始
    ret = start_dma_transfer(ctx);
    if (ret != 0) {
        close(ctx->file_fd);
        ctx->file_fd = -1;
        return ret;
    }
    
    return 0;
}

int axidma_mm2s_play_memory(axidma_mm2s_t *ctx, const void *buffer, uint64_t size)
{
    if (!ctx || !buffer || size == 0) {
        return -EINVAL;
    }
    
    ctx->file_fd = -1;  // ファイルは使用しない
    ctx->file_size = size;
    ctx->current_offset = 0;
    ctx->file_completed = false;
    
    // バッファサイズを計算
    ssize_t half_buffer = ctx->buffer_size / 2;
    ssize_t bytes_to_write;
    
    // データサイズに応じて場合分けして処理
    if (size <= half_buffer) {
        // ケース1: データがバッファの半分以下なら、2 つのバッファに複製する
        bytes_to_write = size;
        
        // 1 つ目のバッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer, bytes_to_write, ctx->buffer_addr) != bytes_to_write) {
            perror("Failed to write to first buffer");
            return -errno;
        }
        
        // 2 つ目のバッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer, bytes_to_write, ctx->buffer_addr + half_buffer) != bytes_to_write) {
            perror("Failed to write to second buffer");
            return -errno;
        }
    } else if (size <= ctx->buffer_size) {
        // ケース2: データがバッファ全体以下なら、2 つの半バッファにそれぞれ書き込む
        ssize_t first_half = size / 2;
        ssize_t second_half = size - first_half;
        
        // 1 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer, first_half, ctx->buffer_addr) != first_half) {
            perror("Failed to write to first half buffer");
            return -errno;
        }
        
        // 2 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, (char*)buffer + first_half, second_half, 
                   ctx->buffer_addr + half_buffer) != second_half) {
            perror("Failed to write to second half buffer");
            return -errno;
        }
    } else {
        // ケース3: データがバッファ全体より大きければ、まずバッファ全体に書き込み、残りは割り込み内で処理する
        // 1 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer, half_buffer, ctx->buffer_addr) != half_buffer) {
            perror("Failed to write to first half buffer");
            return -errno;
        }
        
        // 2 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, (char*)buffer + half_buffer, half_buffer, 
                   ctx->buffer_addr + half_buffer) != half_buffer) {
            perror("Failed to write to second half buffer");
            return -errno;
        }
        
        ctx->current_offset = ctx->buffer_size;
    }
    
    // DMA 転送を開始
    int ret = start_dma_transfer(ctx);
    if (ret != 0) {
        return ret;
    }
    
    return 0;
}

int axidma_mm2s_stop(axidma_mm2s_t *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    return stop_dma_transfer(ctx);
}

int axidma_mm2s_wait(axidma_mm2s_t *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    pthread_t intr_thread;
    pthread_create(&intr_thread, NULL, interrupt_handler, ctx);
    
    // スレッドの終了を待つ
    pthread_join(intr_thread, NULL);
    
    return 0;
}

static int setup_descriptors(axidma_mm2s_t *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    // ディスクリプタ領域をクリア
    memset(ctx->desc, 0, 20 * 0x40);
    
    // 1 つ目のディスクリプタを設定
    ctx->desc[0].nxtdesc = (uint32_t)DMA_DESC_BASE_ADDR + 0x40;
    ctx->desc[0].nxtdesc_msb = 0;
    ctx->desc[0].buffer_addr = (uint32_t)ctx->buffer_addr;
    ctx->desc[0].buffer_addr_msb = (uint32_t)(ctx->buffer_addr >> 32);
    ctx->desc[0].control = (ctx->buffer_size / 2) | CONTROL_TXSOF | CONTROL_TXEOF;  // SOF を有効化、サイズはバッファの半分
    ctx->desc[0].status = 0;
    
    // 2 つ目のディスクリプタを設定
    ctx->desc[1].nxtdesc = (uint32_t)DMA_DESC_BASE_ADDR;
    ctx->desc[1].nxtdesc_msb = 0;
    ctx->desc[1].buffer_addr = (uint32_t)(ctx->buffer_addr + ctx->buffer_size / 2);
    ctx->desc[1].buffer_addr_msb = (uint32_t)((ctx->buffer_addr + ctx->buffer_size / 2) >> 32);
    ctx->desc[1].control = (ctx->buffer_size / 2) | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
    ctx->desc[1].status = 0;
    
    return 0;
}

static int load_initial_data(axidma_mm2s_t *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    // バッファサイズを計算
    ssize_t half_buffer = ctx->buffer_size / 2;
    ssize_t bytes_to_read;
    
    // ファイルサイズに応じて場合分けして処理
    if (ctx->file_size <= half_buffer) {
        // ケース1: ファイルがバッファの半分以下なら、2 つのバッファに複製する
        char *buffer = malloc(ctx->file_size);
        if (!buffer) {
            perror("Failed to allocate memory for file buffer");
            return -ENOMEM;
        }
        
        // ファイル内容を読み込む
        bytes_to_read = ctx->file_size;
        if (read(ctx->file_fd, buffer, bytes_to_read) != bytes_to_read) {
            perror("Failed to read file");
            free(buffer);
            return -errno;
        }
        
        // 1 つ目のバッファに書き込む

        lseek(ctx->h2c_fd, ctx->buffer_addr , SEEK_SET);
        if (write(ctx->h2c_fd, buffer, bytes_to_read) != bytes_to_read) {
            perror("Failed to write to first buffer");
            free(buffer);
            return -errno;
        }
		ctx->desc[0].control = bytes_to_read | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
        
        // 2 つ目のバッファに書き込む
        lseek(ctx->h2c_fd, ctx->buffer_addr + half_buffer, SEEK_SET);
        if (write(ctx->h2c_fd, buffer, bytes_to_read) != bytes_to_read) {
            perror("Failed to write to second buffer");
            free(buffer);
            return -errno;
        }
		ctx->desc[1].control = bytes_to_read | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
        
        free(buffer);
        ctx->current_offset = ctx->file_size;
        ctx->file_completed = true;
    } else if (ctx->file_size <= ctx->buffer_size) {
        // ケース2: ファイルがバッファ全体以下なら、2 つの半バッファにそれぞれ書き込む
        char *buffer = malloc(ctx->file_size);
        if (!buffer) {
            perror("Failed to allocate memory for file buffer");
            return -ENOMEM;
        }
        
        // ファイル内容を読み込む
        if (read(ctx->file_fd, buffer, ctx->file_size) != ctx->file_size) {
            perror("Failed to read file");
            free(buffer);
            return -errno;
        }
        
        ssize_t first_half = ctx->file_size / 2;
        ssize_t second_half = ctx->file_size - first_half;
        
        // 1 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer, first_half, ctx->buffer_addr) != first_half) {
            perror("Failed to write to first half buffer");
            free(buffer);
            return -errno;
        }
		ctx->desc[0].control = first_half | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
        
        // 2 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer + first_half, second_half, 
                   ctx->buffer_addr + half_buffer) != second_half) {
            perror("Failed to write to second half buffer");
            free(buffer);
            return -errno;
        }
		ctx->desc[1].control = second_half | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
        
        free(buffer);
        ctx->current_offset = ctx->file_size;
        ctx->file_completed = true;
    } else {
        // ケース3: ファイルがバッファ全体より大きければ、まずバッファ全体に書き込み、残りは割り込み内で処理する
        char *buffer = malloc(ctx->buffer_size);
        if (!buffer) {
            perror("Failed to allocate memory for file buffer");
            return -ENOMEM;
        }
        
        // ファイル内容を読み込む
        if (read(ctx->file_fd, buffer, ctx->buffer_size) != ctx->buffer_size) {
            perror("Failed to read file");
            free(buffer);
            return -errno;
        }
        
        // 1 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer, half_buffer, ctx->buffer_addr) != half_buffer) {
            perror("Failed to write to first half buffer");
            free(buffer);
            return -errno;
        }
		ctx->desc[0].control = half_buffer | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
        
        // 2 つ目の半バッファに書き込む
        if (pwrite(ctx->h2c_fd, buffer + half_buffer, half_buffer, 
                   ctx->buffer_addr + half_buffer) != half_buffer) {
            perror("Failed to write to second half buffer");
            free(buffer);
            return -errno;
        }
		ctx->desc[1].control = half_buffer | CONTROL_TXSOF | CONTROL_TXEOF;  // EOF を有効化、サイズはバッファの半分
        
        free(buffer);
        ctx->current_offset = ctx->buffer_size;
    }
    
    return 0;
}

static int start_dma_transfer(axidma_mm2s_t *ctx)
{
    if (!ctx) {
        return -EINVAL;
    }
    
    ctx->is_running = true;
    ctx->current_desc_idx = 0;
    
    // 割り込み処理スレッドを起動
    pthread_t intr_thread;
    if (pthread_create(&intr_thread, NULL, interrupt_handler, ctx) != 0) {
        perror("Failed to create interrupt thread");
        stop_dma_transfer(ctx);
        return -errno;
    }
    
    // スレッドをデタッチしてバックグラウンドで実行させる
    pthread_detach(intr_thread);
   


    volatile uint32_t *mm2s_dmacr = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_DMACR);
    volatile uint32_t *mm2s_dmasr = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_DMASR);
    volatile uint32_t *mm2s_curdesc = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_CURDESC);
    volatile uint32_t *mm2s_curdesc_msb = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_CURDESC_MSB);
    volatile uint32_t *mm2s_taildesc = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_TAILDESC);
    volatile uint32_t *mm2s_taildesc_msb = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_TAILDESC_MSB);
    
    // DMA コントローラをリセット
    *mm2s_dmacr = DMACR_RESET;
    usleep(1000);  // リセット完了を待つ
    
    // DMA の状態を確認
    if (!(*mm2s_dmasr & DMASR_HALTED)) {
        fprintf(stderr, "DMA not halted after reset\n");
        return -EIO;
    }
    
    // カレントディスクリプタポインタを設定
    *mm2s_curdesc = (uint32_t)DMA_DESC_BASE_ADDR;
    *mm2s_curdesc_msb = 0;
    
    // DMA 制御レジスタを設定
    uint32_t dmacr_val = DMACR_RS | DMACR_IOC_IRQ ;//| DMACR_ERR_IRQ;
    if (ctx->is_cyclic) {
        dmacr_val |= DMACR_CYCLIC;
    }
    
    // DMA 転送を開始
    *mm2s_dmacr = dmacr_val;
    
    
    // テールディスクリプタポインタを設定
    *mm2s_taildesc = (uint32_t)DMA_DESC_BASE_ADDR + 0x40;
    *mm2s_taildesc_msb = 0;

    //// DMA が起動したか確認
    //if (*mm2s_dmasr & DMASR_HALTED) {
    //    fprintf(stderr, "DMA still halted after start\n");
    //    return -EIO;
    //}
    
    return 0;
}

static int stop_dma_transfer(axidma_mm2s_t *ctx)
{
    if (!ctx || !ctx->is_running) {
        return 0;
    }
    
    volatile uint32_t *mm2s_dmacr = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_DMACR);
    volatile uint32_t *mm2s_dmasr = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_DMASR);
    
    // DMA 転送を停止
    *mm2s_dmacr &= ~DMACR_RS;
    
    // DMA の停止を待つ
    int timeout = 100;  // タイムアウト時間、単位：ミリ秒
    while (!(*mm2s_dmasr & DMASR_HALTED) && timeout > 0) {
        usleep(1000);  // 1 ミリ秒待つ
        timeout--;
    }
    
    if (timeout == 0) {
        fprintf(stderr, "Timeout waiting for DMA to halt\n");
        return -ETIMEDOUT;
    }
    
    // ループモードなら、ループモードを無効にする
    if (ctx->is_cyclic) {
        *mm2s_dmacr &= ~DMACR_CYCLIC;
    }
    
    ctx->is_running = false;
    
    return 0;
}

static void* interrupt_handler(void *arg)
{
    axidma_mm2s_t *ctx = (axidma_mm2s_t *)arg;
    uint32_t intr_status;
    uint32_t temp_status;;
    uint64_t half_buffer = ctx->buffer_size / 2;
    char *buffer = NULL;
	uint64_t intpcnt=0;
    
    // ファイルがすでにバッファに完全にロードされていれば、これ以上の処理は不要
    if (ctx->file_completed && !ctx->is_cyclic) {
        return NULL;
    }
    
    // 一時バッファを確保
    if (!ctx->file_completed) {
        buffer = malloc(half_buffer);
        if (!buffer) {
            perror("Failed to allocate memory for interrupt buffer");
            return NULL;
        }
    }
    
    while (ctx->is_running) {
        // 割り込みを待つ
        if (read(ctx->intr_fd, &intr_status, sizeof(intr_status)) != sizeof(intr_status)) {
            if (errno == EINTR) {
                continue;  // シグナルで中断された。待機を続ける
            }
            perror("Failed to read interrupt status");
            break;
        }
		volatile uint32_t *mm2s_curdesc = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_CURDESC);
    	volatile uint32_t *mm2s_curdesc_msb = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_CURDESC_MSB);
        volatile uint32_t *mm2s_dmasr = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_DMASR);
        volatile uint32_t *mm2s_dmacr = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_DMACR);
		volatile uint32_t *mm2s_taildesc = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_TAILDESC);
    	volatile uint32_t *mm2s_taildesc_msb = (volatile uint32_t *)(ctx->dma_dev_vaddr + MM2S_TAILDESC_MSB);
        
		//printf("mm2s_dmasr =%08x\n",*mm2s_dmasr );
        // 割り込み状態をクリア
		temp_status = *mm2s_dmasr ;
        *mm2s_dmasr = DMASR_IOC_IRQ | DMASR_ERR_IRQ;
        
        // エラーを確認
        if (intr_status & DMASR_ERR_IRQ) {
            fprintf(stderr, "DMA error detected: 0x%08x\n", *mm2s_dmasr);
            break;
        }
		if(ctx->desc[0].status & 0x80000000)
		    ctx->desc[0].status = 0x00000000;
		if(ctx->desc[1].status & 0x80000000){
		    ctx->desc[1].status = 0x00000000; //--この行は削除しないこと--- printf("重启DMA\n");
			*mm2s_taildesc = (uint32_t)DMA_DESC_BASE_ADDR + 0x40;
    		*mm2s_taildesc_msb = 0;
		} 
        // IOC 割り込みを処理
        if (temp_status & DMASR_IOC_IRQ) {
		  	//printf("intpcnt =%ld\n",intpcnt++);
            
            // 次のディスクリプタに切り替える
            ctx->current_desc_idx = (ctx->current_desc_idx + 1) % 2;
            
            // ファイルを完全に読み終え、かつループモードでなければ終了
            if (ctx->file_completed && !ctx->is_cyclic) {
                break;
            }
            // ループ再生モードで、ユーザーが任意のキーを押したら終了
            if (ctx->is_cyclic) {
                int key_pressed = wait_for_keypress();
                if (key_pressed) {
                    break;
                }
            }
            
            // ファイルを読み終えていなければ、データの読み込みを続ける
            if (!ctx->file_completed) {
                // 読み込むデータサイズを計算
                uint64_t bytes_left = ctx->file_size - ctx->current_offset;
                ssize_t bytes_to_read = (bytes_left < half_buffer) ? bytes_left : half_buffer;
				//printf(" ctx->current_offset=%d\n",  ctx->current_offset) ;
                // ファイルデータを読み込む
                if (ctx->file_fd > 0) {
                    ssize_t bytes_read = read(ctx->file_fd, buffer, bytes_to_read);
                    if (bytes_read <= 0) {
                        // ファイル読み込みエラーまたは終端
                        ctx->file_completed = true;
                    } else {
                        // 現在書き込むべきバッファアドレスを計算
                        uint64_t buffer_addr = ctx->buffer_addr;
                        if (ctx->current_desc_idx == 0) {
                            buffer_addr += half_buffer;  // 2 つ目のバッファに書き込む
                        }
                        
                        // データをバッファに書き込む
                        if (pwrite(ctx->h2c_fd, buffer, bytes_read, buffer_addr) != bytes_read) {
                            perror("Failed to write to buffer in interrupt handler");
                            break;
                        }
                        
                        ctx->current_offset += bytes_read;
                        
                        // ファイルを読み終えたか確認
                        if (ctx->current_offset >= ctx->file_size) {
                            ctx->file_completed = true;
                            
                            // 単発再生モードなら、ループモードを無効にする
                            if (!ctx->is_cyclic) {
                                *mm2s_dmacr &= ~DMACR_CYCLIC;
                            } else {
                                // ループモードなら、ファイルオフセットをリセット
                                lseek(ctx->file_fd, 0, SEEK_SET);
                                ctx->current_offset = 0;
                                ctx->file_completed = false;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 一時バッファを解放
    if (buffer) {
        free(buffer);
    }
    
    return NULL;
}

static int wait_for_keypress(void)
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    
    // キーボード入力があるかをノンブロッキングで確認
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    
    if (FD_ISSET(STDIN_FILENO, &fds)) {
        char c;
        read(STDIN_FILENO, &c, 1);
        return 1;  // キーボード入力あり
    }
    
    return 0;  // キーボード入力なし
}

