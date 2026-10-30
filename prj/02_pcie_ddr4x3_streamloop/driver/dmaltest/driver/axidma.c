#include "./axidma.h"
#include "./axidma_cfg.h"
/**
 * 1 つの MM2S DMA ディスクリプタの詳細な内容を表示
 * @param desc_ptr ディスクリプタポインタ
 * @param desc_index ディスクリプタのインデックス（表示用）
 */
void print_s2mm_dma_registers(s2mm_dma_t *dma);
void print_mm2s_dma_registers(mm2s_dma_t *mm2s_dma);
void print_mm2s_descriptor(volatile uint32_t *desc_ptr);

// デバイスファイルを開いてメモリをマップ
int init_mm2s_dma(mm2s_dma_t *dma, const char *filename) {
    struct stat st;

    // DMA コントローラを開く
    dma->dma_fd = open(BAR_SPACE_FD, O_RDWR | O_SYNC);
    if (dma->dma_fd < 0) {
        perror("Failed to open DMA controller");
        return -1;
    }

    // DMA レジスタをマップ
    dma->dma_regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                          dma->dma_fd, DMA_DEV_BASE_ADDR);
    if (dma->dma_regs == MAP_FAILED) {
        perror("Failed to map DMA registers");
        close(dma->dma_fd);
        return -1;
    }

    // ディスクリプタ空間をマップ
    dma->desc_base = mmap(NULL, DMA_DESC_ALL_SIZE/2, PROT_READ | PROT_WRITE, MAP_SHARED,
                           dma->dma_fd, DMA_DESC_BASE_ADDR);
    if (dma->desc_base == MAP_FAILED) {
        perror("Failed to map descriptor space");
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // データファイルを開く
    dma->data_fd = open(H2C0_BUFFER_FD, O_RDWR | O_SYNC);
    if (dma->data_fd < 0) {
        perror("Failed to open data device");
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // イベントファイルを開く
    dma->event_fd = open(MM2S_INTERP_FD, O_RDONLY);
    if (dma->event_fd < 0) {
        perror("Failed to open event device");
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // 入力ファイルのサイズを取得
    if (stat(filename, &st) < 0) {
        perror("Failed to get file size");
        close(dma->event_fd);
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }
    dma->file_size = st.st_size;

    // バッファサイズを決定
    dma->buffer_size = (dma->file_size < MM2S_BUFFER_SIZE) ? dma->file_size : MM2S_BUFFER_SIZE;

    // マップではなくメモリバッファを確保
    dma->buffer = malloc(dma->buffer_size);
    if (dma->buffer == NULL) {
        perror("Failed to allocate buffer memory");
        close(dma->event_fd);
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    dma->bytes_sent = 0;
    dma->running = 1;

    // ループモードを使うかどうかを決定
    dma->is_cyclic = (dma->file_size > dma->buffer_size);

    return 0;
}
// ファイルデータをバッファにロード
int load_file_to_buffer(mm2s_dma_t *dma, const char *filename) {
    int fd;
    ssize_t bytes_read = 0;
    size_t total_read = 0;
    char *buf_ptr = (char *)dma->buffer;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open input file");
        return -1;
    }

    // ファイル内容をバッファに読み込む
    while (total_read < dma->buffer_size &&
           (bytes_read = read(fd, buf_ptr + total_read,
                             dma->buffer_size - total_read)) > 0) {
        total_read += bytes_read;
    }

    if (bytes_read < 0) {
        perror("Error reading file");
        close(fd);
        return -1;
    }

    close(fd);

    // バッファを H2C デバイスに書き込む
    ssize_t bytes_written = 0;
    size_t total_written = 0;

    while (total_written < total_read) {
        bytes_written = write(dma->data_fd, buf_ptr + total_written, total_read - total_written);
        if (bytes_written < 0) {
            if (errno == EINTR) continue;
            perror("Error writing to H2C device");
            return -1;
        }
        total_written += bytes_written;
    }

    printf("Loaded %zu bytes from file to buffer and wrote to H2C device\n", total_written);
    return 0;
}
// MM2S ディスクリプタチェーンを設定
int setup_mm2s_descriptors(mm2s_dma_t *dma) {
    uint8_t *mm2s_desc_base = (uint8_t *)dma->desc_base ; // MM2S ディスクリプタは上位 32KB に配置
    uint64_t remaining_size = dma->file_size;
    uint64_t buffer_offset = MM2S_BUFFER_BASEADDR ;
    uint32_t desc_idx = 0;
    uint64_t transfer_size;
    
    // 必要なディスクリプタ数を計算
    if (dma->file_size <= MM2S_ONE_PACKET_SIZE) {
        // ファイルが 1 パケットのサイズより小さければ、ディスクリプタは 1 つで済む
        dma->desc_count = 1;
        transfer_size = dma->file_size;
    } else {
        // MM2S_ONE_PACKET_SIZE のディスクリプタがいくつ必要かを計算
        dma->desc_count = (dma->file_size + MM2S_ONE_PACKET_SIZE - 1) / MM2S_ONE_PACKET_SIZE;
        if (dma->desc_count > MAX_DESC_COUNT) {//MAX_DESC_COUNT はヘッダファイルで定義 = 512
            dma->desc_count = MAX_DESC_COUNT;
        }
        transfer_size = MM2S_ONE_PACKET_SIZE;
    }
    
    printf("Setting up %d MM2S descriptors\n", dma->desc_count);
    
    // 各ディスクリプタを設定
    for (desc_idx = 0; desc_idx < dma->desc_count; desc_idx++) {
        volatile uint8_t *desc = mm2s_desc_base + desc_idx * DESC_SIZE;
        uint64_t next_desc_addr = ((desc_idx + 1) % dma->desc_count) * DESC_SIZE + DMA_DESC_BASE_ADDR ;
        uint64_t buffer_addr = buffer_offset;
        uint32_t control = 0;
       
        // 残りサイズが転送サイズより小さければ、転送サイズを調整
        if (remaining_size < transfer_size) {
            transfer_size = remaining_size;
        }
        
        // 次のディスクリプタアドレスを設定
        *(volatile uint32_t *)(desc + NXTDESC_OFFSET) = next_desc_addr & 0xFFFFFFFC;
        *(volatile uint32_t *)(desc + NXTDESC_MSB_OFFSET) = next_desc_addr >> 32;
        
        // バッファアドレスを設定
        *(volatile uint32_t *)(desc + BUFFER_ADDR_OFFSET) = buffer_addr & 0xFFFFFFFF;
        *(volatile uint32_t *)(desc + BUFFER_ADDR_MSB_OFFSET) = buffer_addr >> 32;
        
        // 制御フィールドを設定
        control = transfer_size & 0x3FFFFFF; // 転送サイズ
        
        // SOF と EOF フラグを設定。注意：設定したパケット長の期間内に必ず tlast 信号が必要
        //if (desc_idx == 0 || buffer_offset == 0) {
        //    control |= CONTROL_TXSOF; // 最初のディスクリプタ、または新しいパケットの開始
        //}
        //if ((desc_idx == (dma->desc_count - 1)) || remaining_size == transfer_size) {
        //    control |= CONTROL_TXEOF; // 最後のディスクリプタ、またはパケットの終了
        //}
        control |= CONTROL_TXSOF ; // 最初のディスクリプタ、または新しいパケットの開始
        control |= CONTROL_TXEOF; // 最後のディスクリプタ、またはパケットの終了
        *(volatile uint32_t *)(desc + CONTROL_OFFSET) = control;

        // ステータスフィールドをクリア
        *(volatile uint32_t *)(desc + STATUS_OFFSET) = 0;
        
        buffer_offset += transfer_size;
        remaining_size -= transfer_size;
        
        if (remaining_size == 0) {
            break;
        }
    }
    
    return 0;
}
// MM2S DMA 転送を開始
int start_mm2s_dma(mm2s_dma_t *dma) {
    uint32_t dmacr, dmasr;
    uint64_t first_desc_addr = DMA_DESC_BASE_ADDR ; // MM2S ディスクリプタの開始アドレス
    uint64_t tail_desc_addr;
    
    // DMA が停止状態になるのを待つ
    dmasr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMASR);
    if (!(dmasr & DMASR_HALTED)) {
        // DMA が停止していなければ、先に停止させる
        dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR);
        dmacr &= ~DMACR_RS;
        *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR) = dmacr;
        
        // DMA の停止を待つ
        do {
            dmasr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMASR);
        } while (!(dmasr & DMASR_HALTED));
    }
    
    // DMA をソフトリセット
    dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR);
    dmacr |= DMACR_RESET;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR) = dmacr;
    
    // リセット完了を待つ
    do {
        dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR);
    } while (dmacr & DMACR_RESET);
    
    // カレントディスクリプタポインタを設定
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_CURDESC) = first_desc_addr & 0xFFFFFFFC;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_CURDESC_MSB) = first_desc_addr >> 32;
    
    // DMA 制御レジスタを設定
    dmacr = DMACR_DLY_IRQ | DMACR_IOC_IRQ | DMACR_ERR_IRQ;
    
    // ループモードが必要な場合
    if (dma->is_cyclic) {
        dmacr |= DMACR_CYCLIC;
    }
    
    // DMA を起動
    dmacr |= DMACR_RS;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR) = dmacr;
    
    // テールディスクリプタポインタを設定
    if (dma->desc_count > 1) {
        tail_desc_addr =  (dma->desc_count - 1) * DESC_SIZE + DMA_DESC_BASE_ADDR ;
    } else {
        tail_desc_addr = DMA_DESC_BASE_ADDR;
    }
    
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_TAILDESC) = tail_desc_addr & 0xFFFFFFFC;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_TAILDESC_MSB) = tail_desc_addr >> 32;
    
    dma->running = 1;
    printf("MM2S DMA started\n");
    
    return 0;
}

// 割り込み処理スレッド
void *mm2s_event_handler_thread(void *arg) {
    mm2s_dma_t *dma = (mm2s_dma_t *)arg;
    uint32_t event_data;
    struct pollfd fds[1];
    
    // poll 構造体を設定
    fds[0].fd = dma->event_fd;
    fds[0].events = POLLIN;
    
    printf("MM2S中断处理线程启动, 使用POLL模式等待中断dma->running=%d, \n", dma->running);

    while (dma->running) {
        // poll でイベントを待つ。タイムアウトは 100ms
        int ret = poll(fds, 1, 100);

        if (ret < 0) {
            // エラー処理
            if (errno == EINTR) {
                continue;  // シグナルで中断された。ループを継続
            }
            perror("MM2S poll error");
            break;
        } else if (ret == 0) {
            // タイムアウト。ループを継続
            continue;
        }
        
        // 読み取り可能なイベントがあるか確認
        if (fds[0].revents & POLLIN) {
            // イベントデータを読み出す
            ssize_t bytes_read = read(dma->event_fd, &event_data, sizeof(event_data));
            if (bytes_read <= 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    continue;
                }
                perror("Error reading MM2S event");
                break;
            }
            
            //printf("Received mm2s interrupt event: 0x%08x\n", event_data);
            
            // DMA の状態を確認
            uint32_t dmasr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMASR);
            
            // 割り込みフラグをクリア
            if (dmasr & DMASR_IOC_IRQ) {
                *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMASR) = DMASR_IOC_IRQ;
                //printf("IOC interrupt cleared\n");
                
                // 送信済みバイト数を更新
                dma->bytes_sent += MM2S_ONE_PACKET_SIZE;
                if (dma->bytes_sent > dma->file_size) {
                    dma->bytes_sent = dma->file_size;
                }
                
                printf("Bytes sent: %08lx / %08lx\n", dma->bytes_sent, dma->file_size);
                
                // すべてのデータを送信済みなら DMA を停止
                if (dma->bytes_sent >= dma->file_size && !dma->is_cyclic) {
                    printf("All data sent, stopping DMA\n");
                    uint32_t dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR);
                    dmacr &= ~DMACR_RS;
                    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR) = dmacr;
                    dma->running = 0;
                    break;
                }
            }
            
            // エラー割り込みを処理
            if (dmasr & DMASR_ERR_IRQ) {
                *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMASR) = DMASR_ERR_IRQ;
                printf("Error mm2s interrupt received, DMA status: 0x%08x\n", dmasr);
                
                // エラーが発生したら MM2S の DMA を停止
                uint32_t dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR);
                dmacr &= ~DMACR_RS;
                *(volatile uint32_t *)((uint8_t *)dma->dma_regs + MM2S_DMACR) = dmacr;
                dma->running = 0;
                break;
            }
        }
        
        // エラーイベントを確認
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            printf("MM2S poll error event: 0x%x\n", fds[0].revents);
            break;
        }
    }
    
    return NULL;
}

// ワークキューを初期化
int init_work_queue(work_queue_t *queue, int capacity) {
    queue->items = (work_item_t *)malloc(capacity * sizeof(work_item_t));
    if (!queue->items) {
        perror("Failed to allocate work queue items");
        return -1;
    }
    
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(queue->items);
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        perror("Failed to initialize not_empty condition");
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        perror("Failed to initialize not_full condition");
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return -1;
    }
    
    return 0;
}

// ワーク項目をエンキュー
int enqueue_work(work_queue_t *queue, void *buffer, size_t size, int frame_index) {
    pthread_mutex_lock(&queue->mutex);
    
    // キューが満杯でなくなるのを待つ
    while (queue->count == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    // ワーク項目を追加
    work_item_t *item = &queue->items[queue->tail];
    item->buffer = buffer;
    item->size = size;
    item->frame_index = frame_index;
    clock_gettime(CLOCK_REALTIME, &item->ts);
    
    // キューを更新
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    // コンシューマに通知
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

// ワーク項目をデキュー
int dequeue_work(work_queue_t *queue, work_item_t *item) {
    pthread_mutex_lock(&queue->mutex);
    
    // キューが空でなくなるのを待つ
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    // ワーク項目を取得
    *item = queue->items[queue->head];
    
    // キューを更新
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    // プロデューサに通知
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

// ワークキューを破棄
void destroy_work_queue(work_queue_t *queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue->items);
}

// S2MM DMA を初期化
int init_s2mm_dma(s2mm_dma_t *dma, const char *output_dir) {
    memset(dma, 0, sizeof(s2mm_dma_t));
    strncpy(dma->output_dir, output_dir, sizeof(dma->output_dir) - 1);

    // DMA コントローラを開く
    dma->dma_fd = open(BAR_SPACE_FD, O_RDWR | O_SYNC);
    if (dma->dma_fd < 0) {
        perror("Failed to open DMA controller");
        return -1;
    }

    // DMA レジスタをマップ
    dma->dma_regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                          dma->dma_fd, DMA_DEV_BASE_ADDR);
    if (dma->dma_regs == MAP_FAILED) {
        perror("Failed to map DMA registers");
        close(dma->dma_fd);
        return -1;
    }

    // ディスクリプタ空間をマップ
    dma->desc_base = mmap(NULL, DMA_DESC_ALL_SIZE/2, PROT_READ | PROT_WRITE, MAP_SHARED,
                           dma->dma_fd, DMA_DESC_BASE_ADDR+DMA_DESC_ALL_SIZE/2);
    if (dma->desc_base == MAP_FAILED) {
        perror("Failed to map descriptor space");
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // データファイルを開く
    dma->data_fd = open(C2H0_BUFFER_FD, O_RDWR | O_SYNC);
    if (dma->data_fd < 0) {
        perror("Failed to open data device");
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // イベントファイルを開く
    dma->event_fd = open(S2MM_INTERP_FD, O_RDONLY);
    if (dma->event_fd < 0) {
        perror("Failed to open event device");
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // バッファサイズとディスクリプタ数を設定
    dma->buffer_size = S2MM_BUFFER_SIZE;
    dma->desc_count = MAX_DESC_COUNT;
    dma->desc_size = DESC_SIZE;
    dma->segment_size = dma->buffer_size / dma->desc_count;
	printf("dma->buffer_size  = 0x%08lx\n", dma->buffer_size);
	printf("dma->desc_count   = 0x%08x\n", dma->desc_count);
	printf("dma->segment_size = 0x%08lx\n", dma->segment_size);

    // マップではなくメモリバッファを確保
    dma->buffer = malloc(dma->buffer_size);
    if (dma->buffer == NULL) {
        perror("Failed to allocate buffer memory");
        close(dma->event_fd);
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // ディスクリプタ状態の配列を確保
    dma->desc_status = (desc_status_t *)calloc(dma->desc_count, sizeof(desc_status_t));
    if (!dma->desc_status) {
        perror("Failed to allocate descriptor status array");
        free(dma->buffer);
        close(dma->event_fd);
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    // ワークキューを初期化
    if (init_work_queue(&dma->work_queue, S2MM_WORK_QUEUE) != 0) {
        free(dma->desc_status);
        free(dma->buffer);
        close(dma->event_fd);
        close(dma->data_fd);
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
        munmap(dma->dma_regs, 4096);
        close(dma->dma_fd);
        return -1;
    }

    dma->running = 1;
    dma->frame_count = 0;

    return 0;
}


// S2MM ディスクリプタを設定
int setup_s2mm_descriptors(s2mm_dma_t *dma) {
    uint8_t *s2mm_desc_base = (uint8_t *)dma->desc_base ; // S2MM ディスクリプタは下位 32KB に配置
    
    printf("Setting up %d S2MM descriptors, segment size: %zu bytes\n", 
           dma->desc_count, dma->segment_size);
    
    // 各ディスクリプタを設定
    for (uint32_t i = 0; i < dma->desc_count; i++) {
        volatile uint8_t *desc = s2mm_desc_base + i * dma->desc_size;
        uint64_t next_desc_addr = ((i + 1) % dma->desc_count) * dma->desc_size + DMA_DESC_ALL_SIZE/2 + DMA_DESC_BASE_ADDR ;
        uint64_t buffer_addr = S2MM_BUFFER_BASEADDR + i * dma->segment_size; // バッファのオフセット
        
        // 次のディスクリプタアドレスを設定
        *(volatile uint32_t *)(desc + NXTDESC_OFFSET) = next_desc_addr & 0xFFFFFFFC;
        *(volatile uint32_t *)(desc + NXTDESC_MSB_OFFSET) = next_desc_addr >> 32;
        
        // バッファアドレスを設定
        *(volatile uint32_t *)(desc + BUFFER_ADDR_OFFSET) = buffer_addr & 0xFFFFFFFF;
        *(volatile uint32_t *)(desc + BUFFER_ADDR_MSB_OFFSET) = buffer_addr >> 32;
        
        // 制御フィールドを設定 - バッファサイズ
        *(volatile uint32_t *)(desc + CONTROL_OFFSET) = dma->segment_size & 0x3FFFFFF;
        
        // ステータスフィールドをクリア
        *(volatile uint32_t *)(desc + STATUS_OFFSET) = 0;
        
        // ディスクリプタの状態を初期化
        dma->desc_status[i].processed = 0;
        dma->desc_status[i].status = 0;
        dma->desc_status[i].bytes_received = 0;
        dma->desc_status[i].is_sof = 0;
        dma->desc_status[i].is_eof = 0;
    }
    
    return 0;
}

// S2MM DMA を起動
int start_s2mm_dma(s2mm_dma_t *dma) {
    uint32_t dmacr, dmasr;
    uint64_t first_desc_addr = DMA_DESC_BASE_ADDR + DMA_DESC_ALL_SIZE/2; // S2MM ディスクリプタの開始アドレス
    uint64_t tail_desc_addr = (dma->desc_count - 1) * dma->desc_size + first_desc_addr;
    // DMA が停止状態になるのを待つ
    dmasr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMASR);
    if (!(dmasr & DMASR_HALTED)) {
        // DMA が停止していなければ、先に停止させる
        dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMACR);
        dmacr &= ~DMACR_RS;
        *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMACR) = dmacr;
        
        // DMA の停止を待つ
        do {
            dmasr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMASR);
        } while (!(dmasr & DMASR_HALTED));
    }
    
    // DMA をソフトリセット
    dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMACR);
    dmacr |= DMACR_RESET;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMACR) = dmacr;
    
    // リセット完了を待つ
    do {
        dmacr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMACR);
    } while (dmacr & DMACR_RESET);
    
    // カレントディスクリプタポインタを設定
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_CURDESC)     = first_desc_addr & 0xFFFFFFFC;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_CURDESC_MSB) = first_desc_addr >> 32;
    
    // DMA 制御レジスタを設定 - ループモードと割り込みを有効化
    dmacr = DMACR_CYCLIC | DMACR_IOC_IRQ | DMACR_ERR_IRQ | DMACR_DLY_IRQ;
    
    // DMA を起動
    dmacr |= DMACR_RS;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMACR) = dmacr;
    
    // テールディスクリプタポインタを設定
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_TAILDESC) = tail_desc_addr & 0xFFFFFFFC;
    *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_TAILDESC_MSB) = tail_desc_addr >> 32;
   
    dma->running = 1;
    printf("S2MM DMA started in cyclic mode\n");
	//print_s2mm_dma_registers(dma);

    return 0;
}

// S2MM DMA のリソースを後片付け
void cleanup_s2mm_dma(s2mm_dma_t *dma) {
    if (dma->desc_status) {
        free(dma->desc_status);
    }

    if (dma->buffer) {
        free(dma->buffer);
    }

    if (dma->desc_base != MAP_FAILED) {
        munmap(dma->desc_base, DMA_DESC_ALL_SIZE);
    }

    if (dma->dma_regs != MAP_FAILED) {
        munmap(dma->dma_regs, 4096);
    }

    if (dma->event_fd >= 0) {
        close(dma->event_fd);
    }

    if (dma->data_fd >= 0) {
        close(dma->data_fd);
    }

    if (dma->dma_fd >= 0) {
        close(dma->dma_fd);
    }

    destroy_work_queue(&dma->work_queue);
}

// 受信したフレームをファイルに保存
int save_frame_to_file(s2mm_dma_t *dma, void *buffer, size_t size, int frame_index) {
    char filename[512];
    FILE *fp;
    
    // ファイル名を作成
    snprintf(filename, sizeof(filename), "%s/frame_%06d.dat", dma->output_dir, frame_index);
    
    // ファイルを開く
    fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open output file");
        return -1;
    }
    
    // データを書き込む
    if (fwrite(buffer, 1, size, fp) != size) {
        perror("Failed to write data to file");
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    printf("Saved frame %d to %s, size: %zu bytes\n", frame_index, filename, size);
    
    return 0;
}

// ワーカースレッド - ワークキュー内の項目を処理
void *worker_thread(void *arg) {
    s2mm_dma_t *dma = (s2mm_dma_t *)arg;
    work_item_t item;
    
    printf("Worker thread started\n");
    
    while (dma->running) {
        // キューからワーク項目を取得
        if (dequeue_work(&dma->work_queue, &item) == 0) {
            // フレームをファイルに保存
            save_frame_to_file(dma, item.buffer, item.size, item.frame_index);
        }
    }
    
    // キューに残っているワーク項目を処理
    while (dma->work_queue.count > 0) {
        if (dequeue_work(&dma->work_queue, &item) == 0) {
            save_frame_to_file(dma, item.buffer, item.size, item.frame_index);
        }
    }
    
    printf("Worker thread exiting\n");
    return NULL;
}
// S2MM ディスクリプタの状態を処理
void process_descriptors(s2mm_dma_t *dma) {
    uint8_t *s2mm_desc_base = (uint8_t *)dma->desc_base;
    int sof_index = -1;
    int eof_index = -1;
    int found_frame = 0;

    // 前回チェックした位置を static 変数で記憶
    static int last_checked_index = 0;

    // 前回チェックした位置から、ディスクリプタを一周走査
    int i = last_checked_index;
    uint32_t checked_count = 0;

    while (checked_count < dma->desc_count) {
        uint8_t *desc = s2mm_desc_base + i * dma->desc_size;
        uint32_t status = *(uint32_t *)(desc + STATUS_OFFSET);

        // ディスクリプタが完了済みかつ未処理なら
        if ((status & STATUS_COMPLETED) && !dma->desc_status[i].processed) {
            // ディスクリプタの状態を更新
            dma->desc_status[i].status = status;
            dma->desc_status[i].bytes_received = status & 0x3FFFFFF; // 受信したバイト数を取り出す
            dma->desc_status[i].is_sof = (status & STATUS_RXSOF) ? 1 : 0;
            dma->desc_status[i].is_eof = (status & STATUS_RXEOF) ? 1 : 0;

            // SOF フラグを確認
            if (dma->desc_status[i].is_sof) {
                sof_index = i;
            }

            // EOF フラグを確認
            if (dma->desc_status[i].is_eof && sof_index >= 0) {
                eof_index = i;
                found_frame = 1;
                // 次回チェック開始位置を EOF の次のディスクリプタに更新
                last_checked_index = (i + 1) % dma->desc_count;
                break;
            }
        }

        // 次のディスクリプタへ移動
        i = (i + 1) % dma->desc_count;
        checked_count++;
    }

    // 完全なフレームが見つかった場合
    if (found_frame) {
        size_t frame_size = 0;
        void *frame_buffer = NULL;

        // フレームサイズを計算
        if (sof_index <= eof_index || (sof_index > eof_index && sof_index > last_checked_index)) {
            // フレームが連続したディスクリプタ内にある、または SOF がリングバッファの後半にある
            int start = sof_index;
            int end = eof_index;

            if (sof_index > eof_index) {
                // フレームがリングバッファの境界をまたいでいる
                end = eof_index + dma->desc_count;
            }

            // フレームの合計サイズを計算
            for (int j = start; j <= end; j++) {
                int idx = j % dma->desc_count;
                frame_size += dma->desc_status[idx].bytes_received;
            }

            // フレームバッファを確保
            frame_buffer = malloc(frame_size);
            if (!frame_buffer) {
                perror("Failed to allocate frame buffer");
                return;
            }

            // C2H デバイスからデータを読み出す
            size_t offset = 0;
            for (int j = start; j <= end; j++) {
                int idx = j % dma->desc_count;
                size_t bytes = dma->desc_status[idx].bytes_received;
                off_t device_offset = idx * dma->segment_size;

                // lseek で読み出し位置を設定
                if (lseek(dma->data_fd, device_offset, SEEK_SET) == -1) {
                    perror("Failed to seek in C2H device");
                    free(frame_buffer);
                    return;
                }

                // デバイスからデータを読み出す
                ssize_t bytes_read = 0;
                size_t total_read = 0;

                while (total_read < bytes) {
                    bytes_read = read(dma->data_fd, (uint8_t *)frame_buffer + offset + total_read, bytes - total_read);
                    if (bytes_read < 0) {
                        if (errno == EINTR) continue;
                        perror("Error reading from C2H device");
                        free(frame_buffer);
                        return;
                    }
                    total_read += bytes_read;
                }

                offset += bytes;

                // ディスクリプタを処理済みにマーク
                dma->desc_status[idx].processed = 1;
            }
        }

        // ワーク項目をキューに投入
        dma->frame_count++;
        enqueue_work(&dma->work_queue, frame_buffer, frame_size, dma->frame_count);
        printf("Enqueued frame %d, size: %zu bytes (SOF: %d, EOF: %d)\n",
               dma->frame_count, frame_size, sof_index, eof_index);
    } else if (checked_count >= dma->desc_count) {
        // すべてのディスクリプタを確認しても完全なフレームが見つからなければ、チェック位置をリセット
        // これにより異常時に処理が詰まるのを防げる
        last_checked_index = 0;
    }
}

// 割り込み処理スレッドを起動
// MM2S 割り込み処理スレッド

// S2MM 割り込み処理スレッド
void *s2mm_event_handler_thread(void *arg) {
    s2mm_dma_t *dma = (s2mm_dma_t *)arg;
    uint32_t event_data;
    struct pollfd fds[1];
    
    // poll 構造体を設定
    fds[0].fd = dma->event_fd;
    fds[0].events = POLLIN;
    
    printf("S2MM中断处理线程启动, 使用POLL模式等待中断\n");
    
    while (dma->running) {
        // poll でイベントを待つ。タイムアウトは 100ms
        int ret = poll(fds, 1, 100);

        if (ret < 0) {
            // エラー処理
            if (errno == EINTR) {
                continue;  // シグナルで中断された。ループを継続
            }
            perror("S2MM poll error");
            break;
        } else if (ret == 0) {
            // タイムアウト。ループを継続
            continue;
        }
        
        // 読み取り可能なイベントがあるか確認
        if (fds[0].revents & POLLIN) {
            // イベントデータを読み出す
            ssize_t bytes_read = read(dma->event_fd, &event_data, sizeof(event_data));
            if (bytes_read <= 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    continue;
                }
                perror("Error reading S2MM event");
                break;
            }
            
            //printf("Received s2mm interrupt event: 0x%08x\n", event_data);
            
            // DMA の状態を確認
            uint32_t dmasr = *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMASR);
            
            // 割り込みフラグをクリア
            if (dmasr & DMASR_IOC_IRQ) {
                *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMASR) = DMASR_IOC_IRQ;
                printf("IOC interrupt cleared\n");
                
                // ディスクリプタを処理
                process_descriptors(dma);
            }
            
            // エラー割り込みを処理
            if (dmasr & DMASR_ERR_IRQ) {
                *(volatile uint32_t *)((uint8_t *)dma->dma_regs + S2MM_DMASR) = DMASR_ERR_IRQ;
                printf("Error s2mm interrupt received, DMA status: 0x%08x\n", dmasr);
                
                // エラーが発生した場合、DMA を停止するかどうかを選べる
                // ここでは実行を継続し、エラーを記録するだけにする
            }
        }
        
        // エラーイベントを確認
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            printf("S2MM poll error event: 0x%x\n", fds[0].revents);
            break;
        }
    }
    
    return NULL;
}

// メイン関数
int main(int argc, char *argv[]) {
    mm2s_dma_t mm2s_dma;
    s2mm_dma_t s2mm_dma;
    int ret;
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input file> <output_directory>\n", argv[0]);
        return 1;
    }
   


    // 出力ディレクトリが存在するか確認
    struct stat st = {0};
    if (stat(argv[2], &st) == -1) {
        // ディレクトリが存在しないので作成する
        if (mkdir(argv[2], 0755) == -1) {
            perror("Failed to create output directory");
            return 1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s is not a directory\n", argv[2]);
        return 1;
    }
    
	//----printf("initial s2mm !\n");

    //----// DMA を初期化
    //----ret = init_s2mm_dma(&s2mm_dma, argv[2]);
    //----if (ret < 0) {
    //----    fprintf(stderr, "Failed to initialize S2MM DMA\n");
    //----    return 1;
    //----}
    //----
    //----// S2MM ディスクリプタを設定
    //----ret = setup_s2mm_descriptors(&s2mm_dma);
    //----if (ret < 0) {
    //----    fprintf(stderr, "Failed to setup S2MM descriptors\n");
    //----    cleanup_s2mm_dma(&s2mm_dma);
    //----    return 1;
    //----}
    //----
    //----// ワーカースレッドを作成
    //----ret = pthread_create(&s2mm_dma.worker_thread, NULL, worker_thread, &s2mm_dma);
    //----if (ret < 0) {
    //----    perror("Failed to create worker thread");
    //----    cleanup_s2mm_dma(&s2mm_dma);
    //----    return 1;
    //----}
    //----
    //----// イベント処理スレッドを作成
    //----ret = pthread_create(&s2mm_dma.event_thread, NULL, s2mm_event_handler_thread, &s2mm_dma);
    //----if (ret < 0) {
    //----    perror("Failed to create event thread");
    //----    s2mm_dma.running = 0;
    //----    pthread_join(s2mm_dma.worker_thread, NULL);
    //----    cleanup_s2mm_dma(&s2mm_dma);
    //----    return 1;
    //----}
    //----
    //----// DMA 転送を開始
    //----ret = start_s2mm_dma(&s2mm_dma);
    //----if (ret < 0) {
    //----    fprintf(stderr, "Failed to start S2MM DMA\n");
    //----    s2mm_dma.running = 0;
    //----    pthread_join(s2mm_dma.event_thread, NULL);
    //----    pthread_join(s2mm_dma.worker_thread, NULL);
    //----    cleanup_s2mm_dma(&s2mm_dma);
    //----    return 1;
    //----}
   

	printf("S2MM ready!\n");



    // MM2S DMA を初期化
    ret = init_mm2s_dma(&mm2s_dma, argv[1]);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize DMA\n");
        return 1;
    }
    
    // ファイルをバッファにロード
    ret = load_file_to_buffer(&mm2s_dma, argv[1]);
    if (ret < 0) {
        fprintf(stderr, "Failed to load file to buffer\n");
        return 1;
    }
    
	printf("setup mm2s descriptors!\n");
    // MM2S ディスクリプタを設定
    ret = setup_mm2s_descriptors(&mm2s_dma);
    if (ret < 0) {
        fprintf(stderr, "Failed to setup MM2S descriptors\n");
        return 1;
    }
    
    // イベント処理スレッドを作成
    ret = pthread_create(&mm2s_dma.event_thread, NULL, mm2s_event_handler_thread, &mm2s_dma);
    if (ret < 0) {
        perror("Failed to create event thread");
        return 1;
    }
    
    // DMA 転送を開始
    ret = start_mm2s_dma(&mm2s_dma);
    if (ret < 0) {
        fprintf(stderr, "Failed to start MM2S DMA\n");
        return 1;
    }
   
	//for(int i = 0; i < 100000; i++) {
	//	volatile uint32_t *desc_ptr = s2mm_dma.desc_base;
	//	uint32_t status = desc_ptr[7];
    //	printf("  - 解码错误: %s\n", (status & 0x40000000) ? "有错误" : "无错误");
    //	printf("  - 从属错误: %s\n", (status & 0x20000000) ? "有错误" : "无错误");
    //	printf("  - 内部错误: %s\n", (status & 0x10000000) ? "有错误" : "无错误");
    //	printf("  - 实际传输长度: %u 字节\n", status & 0x3FFFFFF);
	//}
    // イベント処理スレッドの終了を待つ
    pthread_join(mm2s_dma.event_thread, NULL);
    printf("MM2S DMA is done. \n");







    
    //----// DMA を停止
    //----s2mm_dma.running = 0;
    //----
    //----// DMA コントローラを停止
    //----uint32_t dmacr = *(volatile uint32_t *)((uint8_t *)s2mm_dma.dma_regs + S2MM_DMACR);
    //----dmacr &= ~DMACR_RS;
    //----*(volatile uint32_t *)((uint8_t *)s2mm_dma.dma_regs + S2MM_DMACR) = dmacr;
    //----
    //----printf("waiting s2mm dma done ..... \n");
    //----// スレッドの終了を待つ
    //----pthread_join(s2mm_dma.event_thread, NULL);
    //----pthread_join(s2mm_dma.worker_thread, NULL);
    //----
    //----// リソースを後片付け
    //----cleanup_s2mm_dma(&s2mm_dma);

    // mm2s のリソースを後片付け
    if (mm2s_dma.buffer) {
        free(mm2s_dma.buffer);
    }
    munmap(mm2s_dma.desc_base, DMA_DESC_ALL_SIZE);
    munmap(mm2s_dma.dma_regs, 4096);
    close(mm2s_dma.event_fd);
    close(mm2s_dma.data_fd);
    close(mm2s_dma.dma_fd);
    
    printf("DMA transfer completed\n");
    printf("S2MM DMA stopped. Received %d frames.\n", s2mm_dma.frame_count);
    return 0;
}



/**
 * MM2S DMA コントローラのレジスタとディスクリプタ情報を表示
 * @param mm2s_dma MM2S DMA コントローラ構造体へのポインタ
 * @param verbose 詳細情報を表示するかどうか
 */
void print_mm2s_dma_registers(mm2s_dma_t *mm2s_dma)
{
    if (!mm2s_dma) {
        printf("[ERROR] MM2S DMA结构体指针为NULL\n");
        return;
    }

    volatile uint32_t *dma_regs = mm2s_dma->dma_regs;
    volatile uint32_t *desc_base = mm2s_dma->desc_base;

    if (!dma_regs) {
        printf("[ERROR] MM2S DMA寄存器指针为NULL\n");
        return;
    }

    printf("\n===== MM2S DMA寄存器状态 =====\n");

    // 制御レジスタ (MM2S_DMACR, 0x00)
    uint32_t dmacr = dma_regs[0x00/4];
    printf("MM2S控制寄存器(0x00): 0x%08x\n", dmacr);
    printf("  - 运行状态: %s\n", (dmacr & 0x1) ? "启用" : "禁用");
    printf("  - 复位状态: %s\n", (dmacr & 0x4) ? "复位中" : "正常");
    printf("  - Keyhole模式: %s\n", (dmacr & 0x8) ? "启用" : "禁用");
    printf("  - 循环模式: %s\n", (dmacr & 0x10) ? "启用" : "禁用");
    printf("  - IOC_IrqEn: %s\n", (dmacr & 0x1000) ? "启用" : "禁用");
    printf("  - Dly_IrqEn: %s\n", (dmacr & 0x2000) ? "启用" : "禁用");
    printf("  - Err_IrqEn: %s\n", (dmacr & 0x4000) ? "启用" : "禁用");
    printf("  - IRQThreshold: %u\n", (dmacr >> 16) & 0xFF);
    printf("  - IRQDelay: %u\n", (dmacr >> 24) & 0xFF);

    // ステータスレジスタ (MM2S_DMASR, 0x04)
    uint32_t dmasr = dma_regs[0x04/4];
    printf("MM2S状态寄存器(0x04): 0x%08x\n", dmasr);
    printf("  - DMA引擎状态: %s\n", (dmasr & 0x1) ? "停止" : "运行中");
    printf("  - 空闲状态: %s\n", (dmasr & 0x2) ? "空闲" : "忙碌");
    printf("  - DMA内部错误: %s\n", (dmasr & 0x10) ? "有错误" : "无错误");
    printf("  - DMA从属错误: %s\n", (dmasr & 0x20) ? "有错误" : "无错误");
    printf("  - DMA解码错误: %s\n", (dmasr & 0x40) ? "有错误" : "无错误");
    printf("  - SG内部错误: %s\n", (dmasr & 0x100) ? "有错误" : "无错误");
    printf("  - SG从属错误: %s\n", (dmasr & 0x200) ? "有错误" : "无错误");
    printf("  - SG解码错误: %s\n", (dmasr & 0x400) ? "有错误" : "无错误");
    printf("  - IOC中断: %s\n", (dmasr & 0x1000) ? "已触发" : "未触发");
    printf("  - 延迟中断: %s\n", (dmasr & 0x2000) ? "已触发" : "未触发");
    printf("  - 错误中断: %s\n", (dmasr & 0x4000) ? "已触发" : "未触发");
    printf("  - IRQThresholdStatus: %u\n", (dmasr >> 16) & 0xFF);
    printf("  - IRQDelayStatus: %u\n", (dmasr >> 24) & 0xFF);

    // カレントディスクリプタアドレス
    printf("MM2S当前描述符地址(0x0C+0x08): 0x%08x%08x\n", dma_regs[0x0C/4], dma_regs[0x08/4]);
    // テールディスクリプタアドレス
    printf("MM2S最尾描述符地址(0x14+0x10): 0x%08x%08x\n", dma_regs[0x14/4], dma_regs[0x10/4]);

    // ディスクリプタ情報
    if (desc_base) {
        if (mm2s_dma->desc_count > 1) {
            printf("\n===== MM2S描述符 =====\n");
            //for (uint32_t i = 1; i < dma->desc_count; i++) {
            for (uint32_t i = 0; i < 3; i++) {
                volatile uint32_t *next_desc = desc_base + (i * 16); // 各ディスクリプタが 32 ビットワード 8 個分を占めると仮定
                printf("描述符 #%d 地址: %p\n", i, (void*)next_desc);
                printf("  [0] 第%d的下一个描述符地址: 0x%08x%08x\n",i,next_desc[1], next_desc[0]);
                printf("  [1] 第%d的下一个缓冲区地址: 0x%08x%08x\n",i, next_desc[3], next_desc[2]);
				uint32_t control = next_desc[6];
        		printf("  [2] 控制: 0x%08x\n", control);
        		printf("      - 缓冲区长度: %u 字节\n", control & 0x3FFFFFF);
        		printf("      - SOP: %s\n", (control & 0x8000000) ? "是" : "否");
        		printf("      - EOP: %s\n", (control & 0x4000000) ? "是" : "否");
        		uint32_t status = desc_base[7];
        		printf("  [3] 状态: 0x%08x\n", status);
        		printf("      - 完成状态: %s\n", (status & 0x80000000) ? "已完成" : "未完成");
        		printf("      - 解码错误: %s\n", (status & 0x40000000) ? "有错误" : "无错误");
        		printf("      - 从属错误: %s\n", (status & 0x20000000) ? "有错误" : "无错误");
        		printf("      - 内部错误: %s\n", (status & 0x10000000) ? "有错误" : "无错误");
        		printf("      - 实际传输长度: %u 字节\n", status & 0x3FFFFFF);
            }
        }
    } else {
        printf("[警告] S2MM描述符基地址为NULL\n");
    }

    printf("\n");
}

void print_s2mm_dma_registers(s2mm_dma_t *dma)
{
    if (!dma) {
        printf("[ERROR] S2MM DMA结构体指针为NULL\n");
        return;
    }

    volatile uint32_t *dma_regs  = dma->dma_regs;
    volatile uint32_t *desc_base = dma->desc_base;

    if (!dma_regs) {
        printf("[ERROR] S2MM DMA寄存器指针为NULL\n");
        return;
    }

    printf("\n===== S2MM DMA寄存器状态 =====\n");

    // 制御レジスタ (S2MM_DMACR, 0x00)
    uint32_t dmacr = dma_regs[0x30/4];
    printf("S2MM控制寄存器(0x30): 0x%08x\n", dmacr);
    printf("  - 运行状态: %s\n", (dmacr & 0x1) ? "启用" : "禁用");
    printf("  - 复位状态: %s\n", (dmacr & 0x4) ? "复位中" : "正常");
    printf("  - Keyhole模式: %s\n", (dmacr & 0x8) ? "启用" : "禁用");
    printf("  - 循环模式: %s\n", (dmacr & 0x10) ? "启用" : "禁用");
    printf("  - IOC_IrqEn: %s\n", (dmacr & 0x1000) ? "启用" : "禁用");
    printf("  - Dly_IrqEn: %s\n", (dmacr & 0x2000) ? "启用" : "禁用");
    printf("  - Err_IrqEn: %s\n", (dmacr & 0x4000) ? "启用" : "禁用");
    printf("  - IRQThreshold: %u\n", (dmacr >> 16) & 0xFF);
    printf("  - IRQDelay: %u\n", (dmacr >> 24) & 0xFF);

    // ステータスレジスタ (S2MM_DMASR, 0x04)
    uint32_t dmasr = dma_regs[0x34/4];
    printf("S2MM状态寄存器(0x34): 0x%08x\n", dmasr);
    printf("  - DMA引擎状态: %s\n", (dmasr & 0x1) ? "停止" : "运行中");
    printf("  - 空闲状态: %s\n", (dmasr & 0x2) ? "空闲" : "忙碌");
    printf("  - DMA内部错误: %s\n", (dmasr & 0x10) ? "有错误" : "无错误");
    printf("  - DMA从属错误: %s\n", (dmasr & 0x20) ? "有错误" : "无错误");
    printf("  - DMA解码错误: %s\n", (dmasr & 0x40) ? "有错误" : "无错误");
    printf("  - SG内部错误: %s\n", (dmasr & 0x100) ? "有错误" : "无错误");
    printf("  - SG从属错误: %s\n", (dmasr & 0x200) ? "有错误" : "无错误");
    printf("  - SG解码错误: %s\n", (dmasr & 0x400) ? "有错误" : "无错误");
    printf("  - IOC中断: %s\n", (dmasr & 0x1000) ? "已触发" : "未触发");
    printf("  - 延迟中断: %s\n", (dmasr & 0x2000) ? "已触发" : "未触发");
    printf("  - 错误中断: %s\n", (dmasr & 0x4000) ? "已触发" : "未触发");
    printf("  - IRQThresholdStatus: %u\n", (dmasr >> 16) & 0xFF);
    printf("  - IRQDelayStatus: %u\n", (dmasr >> 24) & 0xFF);

    // カレントディスクリプタアドレス
    printf("MM2S当前描述符地址(0x3C+0x38): 0x%08x%08x\n", dma_regs[0x3C/4], dma_regs[0x38/4]);
    // テールディスクリプタアドレス
    printf("MM2S最尾描述符地址(0x44+0x40): 0x%08x%08x\n", dma_regs[0x44/4], dma_regs[0x40/4]);


    // ディスクリプタ情報
    if (desc_base) {
        if (dma->desc_count > 1) {
            printf("\n===== S2MM描述符 =====\n");
            //for (uint32_t i = 1; i < dma->desc_count; i++) {
            for (uint32_t i = 0; i < 3; i++) {
                volatile uint32_t *next_desc = desc_base + (i * 16); // 各ディスクリプタが 32 ビットワード 8 個分を占めると仮定
                printf("描述符 #%d 地址: %p\n", i, (void*)next_desc);
                printf("  [0] 第%d的下一个描述符地址: 0x%08x%08x\n",i,next_desc[1], next_desc[0]);
                printf("  [1] 第%d的下一个缓冲区地址: 0x%08x%08x\n",i, next_desc[3], next_desc[2]);
				uint32_t control = next_desc[6];
        		printf("  [2] 控制: 0x%08x\n", control);
        		printf("      - 缓冲区长度: %u 字节\n", control & 0x3FFFFFF);
        		printf("      - SOP: %s\n", (control & 0x8000000) ? "是" : "否");
        		printf("      - EOP: %s\n", (control & 0x4000000) ? "是" : "否");
        		uint32_t status = desc_base[7];
        		printf("  [3] 状态: 0x%08x\n", status);
        		printf("      - 完成状态: %s\n", (status & 0x80000000) ? "已完成" : "未完成");
        		printf("      - 解码错误: %s\n", (status & 0x40000000) ? "有错误" : "无错误");
        		printf("      - 从属错误: %s\n", (status & 0x20000000) ? "有错误" : "无错误");
        		printf("      - 内部错误: %s\n", (status & 0x10000000) ? "有错误" : "无错误");
        		printf("      - 实际传输长度: %u 字节\n", status & 0x3FFFFFF);
            }
        }
    } else {
        printf("[警告] S2MM描述符基地址为NULL\n");
    }

    printf("\n");
}
void print_mm2s_descriptor(volatile uint32_t *desc_ptr)
{
    if (!desc_ptr) {
        printf("[ERROR] 描述符指针为NULL\n");
        return;
    }

    printf("\n===== 描述符 =======\n");
    printf("描述符地址: %p\n", (void*)desc_ptr);

    // 次のディスクリプタアドレス
    uint32_t next_desc_lo = desc_ptr[0];
    uint32_t next_desc_hi = desc_ptr[1];
    printf("下一描述符地址: 0x%08x%08x\n", next_desc_hi, next_desc_lo);
    if (next_desc_lo == 0 && next_desc_hi == 0) {
        printf("  (这是链表中的最后一个描述符)\n");
    }

    // バッファアドレス
    uint32_t buffer_addr_lo = desc_ptr[2];
    uint32_t buffer_addr_hi = desc_ptr[3];
    printf("缓冲区地址: 0x%08x%08x\n", buffer_addr_hi, buffer_addr_lo);

    // 制御ワード
    uint32_t control = desc_ptr[6];
    printf("控制字: 0x%08x\n", control);
    printf("  - 缓冲区长度: %u 字节\n", control & 0x3FFFFFF);

    // 制御ビット
    if (control & 0x8000000) printf("  - SOP: 是 (包起始)\n");
    if (control & 0x4000000) printf("  - EOP: 是 (包结束)\n");

    // ステータスワード
    uint32_t status = desc_ptr[7];
    printf("状态字: 0x%08x\n", status);

    // ステータスビット
    printf("  - 完成状态: %s\n", (status & 0x80000000) ? "已完成" : "未完成");

    if (status & 0x80000000) {  // 完了していれば、さらに詳しいステータス情報を表示
        printf("  - 解码错误: %s\n", (status & 0x40000000) ? "有错误" : "无错误");
        printf("  - 从属错误: %s\n", (status & 0x20000000) ? "有错误" : "无错误");
        printf("  - 内部错误: %s\n", (status & 0x10000000) ? "有错误" : "无错误");
        printf("  - 实际传输长度: %u 字节\n", status & 0x3FFFFFF);

        // 転送長が要求長と一致するか確認
        uint32_t requested_len = control & 0x3FFFFFF;
        uint32_t actual_len = status & 0x3FFFFFF;

        if (requested_len != actual_len) {
            printf("  [警告] 实际传输长度(%u)与请求长度(%u)不匹配!\n",
                   actual_len, requested_len);
        }
    }

    // アプリケーションデータ（通常はユーザー定義データに使用）
    printf("应用数据: 0x%08x\n", desc_ptr[7]);

    // 追加のデバッグ情報：ディスクリプタの物理アドレスを計算
    printf("描述符物理地址估计: 0x%lx\n",
           (uintptr_t)desc_ptr & 0xFFFFFFFFFFFFF000UL);  // ページアラインを仮定
}
