#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdarg.h>
#include "xparameters.h"
#include "xiic_l.h"
#include "xiic.h"
#include "xiic_wbwr.h"
#include "lib_wbwr.h"

// FIR 係数の初期値:	coeff_00 <= 16'd54    ;
// FIR 係数の初期値:	coeff_01 <= 16'd159   ;
// FIR 係数の初期値:	coeff_02 <= 16'd344   ;
// FIR 係数の初期値:	coeff_03 <= 16'd671   ;
// FIR 係数の初期値:	coeff_04 <= 16'd1198  ;
// FIR 係数の初期値:	coeff_05 <= 16'd1970  ;
// FIR 係数の初期値:	coeff_06 <= 16'd3009  ;
// FIR 係数の初期値:	coeff_07 <= 16'd4314  ;
// FIR 係数の初期値:	coeff_08 <= 16'd5856  ;
// FIR 係数の初期値:	coeff_09 <= 16'd7574  ;
// FIR 係数の初期値:	coeff_10 <= 16'd9386  ;
// FIR 係数の初期値:	coeff_11 <= 16'd11189 ;
// FIR 係数の初期値:	coeff_12 <= 16'd12871 ;
// FIR 係数の初期値:	coeff_13 <= 16'd14321 ;
// FIR 係数の初期値:	coeff_14 <= 16'd15438 ;
// FIR 係数の初期値:	coeff_15 <= 16'd16143 ;
// FIR 係数の初期値:	coeff_16 <= 16'd16384 ;
// FIR 係数の初期値:	coeff_17 <= 16'd16143 ;
// FIR 係数の初期値:	coeff_18 <= 16'd15438 ;
// FIR 係数の初期値:	coeff_19 <= 16'd14321 ;
// FIR 係数の初期値:	coeff_20 <= 16'd12871 ;
// FIR 係数の初期値:	coeff_21 <= 16'd11189 ;
// FIR 係数の初期値:	coeff_22 <= 16'd9386  ;
// FIR 係数の初期値:	coeff_23 <= 16'd7574  ;
// FIR 係数の初期値:	coeff_24 <= 16'd5856  ;
// FIR 係数の初期値:	coeff_25 <= 16'd4314  ;
// FIR 係数の初期値:	coeff_26 <= 16'd3009  ;
// FIR 係数の初期値:	coeff_27 <= 16'd1970  ;
// FIR 係数の初期値:	coeff_28 <= 16'd1198  ;
// FIR 係数の初期値:	coeff_29 <= 16'd671   ;
// FIR 係数の初期値:	coeff_30 <= 16'd344   ;
// FIR 係数の初期値:	coeff_31 <= 16'd159   ;
// FIR 係数の初期値:	coeff_32 <= 16'd54    ;

#define IIC_DEV_ADDR 0x00

off_t base_offset = 0x00800000;  // ベースアドレスのオフセット
size_t mapping_size = 64 * 1024; // マップするサイズ。64K

int reset_fir_module(UINTPTR baseaddr);


int main(void) {
    int axilte;
	int i;
    void *mapped_base;
	UINTPTR base_address;
	u8 rxdata[16];
	u8 txdata[16];
	u16 readback[1];

	for(i = 0; i < 16; i++)
	{
		rxdata[i] = 0;
		txdata[i] = 0;
	}

    // デバイスファイルを開く
    axilte = open("/dev/xdma0_user", O_RDWR | O_SYNC);//axi lite デバイス
    if (axilte == -1) {
        perror("Failed to open device");
		close(axilte);
        return -1;
    }

    // デバイスファイルをメモリにマップ
	mapped_base = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, axilte, base_offset);
    if (mapped_base == MAP_FAILED) {
        perror("Failed to map memory");
		close(axilte);
        return -1;
    }

    // マップしたアドレスを指定型のポインタに変換
    base_address = (UINTPTR)mapped_base;
	// モジュールをリセット
	reset_fir_module(base_address);


    // base_address を使って操作する
	unsigned readbyte ;
	AddressType wishboneAddr = 0x00;
	u16 ByteCount = 2;
	int status ;
	u16 txdat;
	printf("begin wishbone read\n");
	Initialization_IIC(base_address);
	for(i = 0; i < 1; i++)
	{
		//txdat = (u16) 1000;
		//printf("CR = %x\n", XIic_ReadReg(base_address,XIIC_CR_REG_OFFSET));
		//status = wishbone_write(base_address, IIC_DEV_ADDR , i, &txdat);
		status = wishbone_read(base_address, IIC_DEV_ADDR , i, readback);
		//status = Single_Cell_Read(base_address, IIC_DEV_ADDR , i, readback);
		printf("wishbone addr[%02d] = %04d with status = %d\n", i, readback[0], status);
	}
    // メモリマップを解除してファイルを閉じる
    munmap(mapped_base, mapping_size);
    close(axilte);

    return 0;
}

int reset_fir_module(UINTPTR baseaddr)
{
	usleep(10); Xil_Out32(baseaddr + XIIC_GPO_REG_OFFSET , 0x00000000);//1. IIC 経由で周辺回路をリセット
	usleep(10); Xil_Out32(baseaddr + XIIC_RESETR_OFFSET	 , 0x0000000a);//2. IIC モジュール全体をリセット
	usleep(10); Xil_Out32(baseaddr + XIIC_GPO_REG_OFFSET , 0x000000ff); //3. IIC 周辺回路の FIR モジュールを有効化
	return 0;
}

