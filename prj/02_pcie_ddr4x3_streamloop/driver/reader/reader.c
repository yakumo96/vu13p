//========================================================================
//        author   : masw
//        email    : masw@masw.tech     
//        creattime: 2025年03月13日 木曜日 15時50分09秒
//========================================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define C2H_DEVICE "/dev/xdma0_c2h_0"  // C2H デバイスファイルのパス
#define BUFFER_SIZE 256                 // 読み出すデータサイズ。256 バイト固定

int main(int argc, char *argv[])
{
    int fd;
    ssize_t bytes_read;
    uint8_t buffer[BUFFER_SIZE];
    uint64_t start_address = 0;

    // コマンドライン引数をチェック
    if (argc != 2) {
        printf("用法: %s <起始地址(十六进制)>\n", argv[0]);
        return -1;
    }

    // 開始アドレス引数を解析
    if (sscanf(argv[1], "0x%lx", &start_address) != 1) {
        printf("错误: 无法解析起始地址 '%s'，请使用十六进制格式 (例如: 0x1000)\n", argv[1]);
        return -1;
    }

    printf("准备从地址 0x%lx 读取 %d 字节的数据\n", start_address, BUFFER_SIZE);

    // C2H デバイスを開く
    fd = open(C2H_DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("无法打开C2H设备");
        return -1;
    }

    // ファイルオフセットを開始アドレスに設定
    if (lseek(fd, start_address, SEEK_SET) == (off_t)-1) {
        perror("设置地址偏移量失败");
        close(fd);
        return -1;
    }

    // C2H チャネルからデータを読み出す
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = read(fd, buffer, BUFFER_SIZE);

    if (bytes_read < 0) {
        perror("读取数据失败");
        close(fd);
        return -1;
    }

    printf("成功读取 %ld 字节数据\n", bytes_read);

    // データを 16 進形式で表示
    printf("数据内容 (十六进制):\n");
    for (int i = 0; i < bytes_read; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");

    // デバイスを閉じる
    close(fd);
    return 0;
}

