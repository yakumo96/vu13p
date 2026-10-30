//========================================================================
//        author   : masw
//        email    : masw@masw.tech     
//        creattime: 2023年08月20日 日曜日 23時25分15秒
//========================================================================

#ifndef __ALEX_IIC_H__
#define __ALEX_IIC_H__


#include <stdint.h>
#include <stddef.h>

typedef int8_t   s8 ;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint8_t  u8 ;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

//typedef intptr_t INTPTR;
//typedef uintptr_t UINTPTR;


#define ALEX_MASK_BUSY      0x00000001//モジュールが I2C 操作を実行中のとき High
#define ALEX_MASK_BUS_CONT  0x00000002//モジュールがアクティブなバスを制御しているとき High
#define ALEX_MASK_BUS_ACT   0x00000004//バスがアクティブなとき High
#define ALEX_MASK_MISS_ACK  0x00000008//スレーブからの ACK パルスが検出されなかったとき High にセット；1 を書き込むとクリア
#define ALEX_MASK_CMD_EMPTY 0x00000100//コマンド FIFO が空
#define ALEX_MASK_CMD_FULL  0x00000200//コマンド FIFO が満杯
#define ALEX_MASK_CMD_OVF   0x00000400//コマンド FIFO オーバーフロー；1 を書き込むとクリア
#define ALEX_MASK_WR_EMPTY  0x00000800//書き込みデータ FIFO が空
#define ALEX_MASK_WR_FULL   0x00001000//書き込みデータ FIFO が満杯
#define ALEX_MASK_WR_OVF    0x00002000//書き込みデータ FIFO オーバーフロー；1 を書き込むとクリア
#define ALEX_MASK_RD_EMPTY  0x00004000//読み出しデータ FIFO が空
#define ALEX_MASK_RD_FULL   0x00008000//読み出しデータ FIFO が満杯


#define ALEX_CMD_START          0x00000100 //：High にすると I2C スタートを発行。書き込むとコマンド FIFO に投入される
#define ALEX_CMD_READ           0x00000200 //：High にすると読み出しを開始。書き込むとコマンド FIFO に投入される
#define ALEX_CMD_WRITE          0x00000400 //：High にすると書き込みを開始。書き込むとコマンド FIFO に投入される
#define ALEX_CMD_WRITE_MULTIPLE 0x00000800 //：High にするとブロック書き込みを開始。書き込むとコマンド FIFO に投入される
#define ALEX_CMD_STOP           0x00000100 //：High にすると I2C ストップを発行。書き込むとコマンド FIFO に投入される

#define ALEX_DATA_VALID         0x00000100 //：有効な読み出しデータであることを示す。16 ビットのアトミックな読み書きアクセスを使う必要がある
#define ALEX_DATA_LAST          0x00000200 //：ブロック書き込み（write_multiple）の最終バイトを示す。16 ビットのアトミックな読み書きアクセスを使う必要がある



u32 alex_read_status(uintptr_t baseaddr);
u32 alex_get_command(uintptr_t baseaddr);
u32 alex_set_command(uintptr_t baseaddr, u32 cmd);
u32 alex_get_fifo(uintptr_t baseaddr);
u32 alex_put_fifo(uintptr_t baseaddr, u32 data);

int alex_iic_wb_read (uintptr_t baseaddr, u8 iicaddr, u8 regaddr, u16 *readback);
int alex_iic_wb_write(uintptr_t baseaddr, u8 iicaddr, u8 regaddr, u16 *readback);

#endif
