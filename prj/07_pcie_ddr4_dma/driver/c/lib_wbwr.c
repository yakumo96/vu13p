/***************************** Include Files *********************************/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdarg.h>
#include "xiic.h"

#include "lib_wbwr.h"

void Initialization_IIC(UINTPTR baseaddr) 
{
	Xil_Out32(baseaddr + XIIC_RFD_REG_OFFSET, 0x0000000f);//Set the RX_FIFO depth
	Xil_Out32(baseaddr + XIIC_CR_REG_OFFSET, Xil_In32(baseaddr + XIIC_CR_REG_OFFSET)|0x00000002);//Reset the TX_FIFO
	Xil_Out32(baseaddr + XIIC_CR_REG_OFFSET, Xil_In32(baseaddr + XIIC_CR_REG_OFFSET)|0x00000001);//Enable the AXI IIC
	Xil_Out32(baseaddr + XIIC_CR_REG_OFFSET, Xil_In32(baseaddr + XIIC_CR_REG_OFFSET)&0xfffffffd);//Remove the TX_FIFO reset
	Xil_Out32(baseaddr + XIIC_CR_REG_OFFSET, Xil_In32(baseaddr + XIIC_CR_REG_OFFSET)&0xffffffbf);//Disable the general call
	printf("Initialization_IIC Device finished.\n\r");
}

// wishbone の読み書き関数を定義する。
// 読み出し関数
// データ buffer を TX_FIFO に書き込む
// レジスタ	値	説明
// TX_FIFO	000001A0	書き込み制御ワード + START
// TX_FIFO	00000000	アドレス 8 ビット
// TX_FIFO	000001A1	読み出し制御ワード + RSTART
// TX_FIFO	00000205	読み出しデータ数 + STOP
// CR	00000025	送信開始

int wishbone_read(UINTPTR baseaddr, u8 iicaddr, u8 regaddr, u16 *readback)
{
	readback[0] = 0;
	u16 buffer[16];
	int i = 0;
	for(i = 0; i < 16; i++)
		buffer[i] = 0;

	// TX_FIFO のアドレスは baseaddr + 0x108 
	// fifo に書き込む前にまず FIFO をクリア
	// 1. 制御ワード + START を書き込む
	// 2. アドレスを書き込む
	// 3. 制御ワード + RSTART を書き込む
	// 4. 読み出しデータ数 + STOP を書き込む
	// 5. 送信開始
	// まず関連レジスタをクリアする
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000002);
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000000);
		printf("-----clear OK SR = 0x%x------\n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) );
	printf("-----bus not busy 0x114 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x114) );
	printf("-----bus not busy 0x118 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x118) );
	printf("-----bus not busy 0x120 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x120) );
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000100 | iicaddr);
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000000 | regaddr);
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000101 | iicaddr);
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000202);
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000025);
		printf("-----set begin OK SR = 0x%x------\n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) );
	// 起動後に SR レジスタを読み、10ms 以内に 0x80 が返れば抜けて成功を返す。タイムアウトした場合は失敗を返す。
	int timeout = 0;
	while (timeout < 50000)
	{
		printf("SR = 0x%x\n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) );
		if ((XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) & 0x88) == 0x88)
			break;
		usleep(5000);
		timeout++;
	}
	//if (timeout > 4)
	//	return -1;//IIC 送信タイムアウトで失敗

	printf("-----bus not busy SR = 0x%x------\n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) );
	printf("-----bus not busy 0x114 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x114) );
	printf("-----bus not busy 0x118 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x118) );
	printf("-----bus not busy 0x120 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x120) );
	//--timeout = 0;
	//while (timeout < 50)
	i = 0;
	while(1)
	{
		if ((XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) & 0x40) == 0x40)
		{
			printf("SR = %x, b[0]=%x,b1=%x, i = %d \n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET), buffer[0], buffer[1],i);
			//XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000000);
			//if ((XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) & 0x08) == 0x00)
			break;
		}
		else
			buffer[i++] = XIic_ReadReg(baseaddr, XIIC_DRR_REG_OFFSET);
		//if(i == 3)
		//	break;
		//usleep(1000);
		//timeout++;
	}
		usleep(10);
	printf("-----bus not busy SR = 0x%x------\n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) );
	printf("-----bus not busy 0x114 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x114) );
	printf("-----bus not busy 0x118 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x118) );
	printf("-----bus not busy 0x120 = 0x%x------\n", XIic_ReadReg(baseaddr, 0x120) );
	for(i = 0; i < 16; i++)
		printf("buffer[%d] = %x\n",i,buffer[i]);
	readback[0] = buffer[0] | buffer[1] << 8;
	//--if (timeout > 4)
	//--	return -3;//IIC 送信タイムアウトで失敗


	//---//XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000000);
	//---//RX FIFO からデータを 2 個読み出す。SR レジスタの RX_FIFO_EMPTY が 1 になるまで。
	//---//int i = 0;
	//---usleep(10000);
	//---u16 lsb = XIic_ReadReg(baseaddr, XIIC_DRR_REG_OFFSET);
	//---usleep(10000);
	//---u16 msb = XIic_ReadReg(baseaddr, XIIC_DRR_REG_OFFSET);
	//---usleep(10000);
	//---//while(!(XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) & 0x40))
	//---//{
	//---//	printf("SR = %x\n", XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET));
	//---//	buffer[0] = XIic_ReadReg(baseaddr, XIIC_DRR_REG_OFFSET);
	//---//	buffer[1] = XIic_ReadReg(baseaddr, XIIC_DRR_REG_OFFSET);
	//---//	//if(i == 17)
	//---//	//	break;
	//---//}
	//---//if (i == 16)
	//---//	return -2; // FIFO の読み出しに失敗
	//---//printf("timeout=%d ,i=%d,   ", timeout, i);
	//---printf("buff[0]=%d ,  ", lsb);
	//---printf("buff[1]=%d  \n", msb);
	//---readback[0] = lsb | msb << 8;
	// 終了後に FIFO をクリア
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000002);
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000000);
	return 0;
}
int wishbone_write(UINTPTR baseaddr, u8 iicaddr, u8 regaddr, u16 *buffer)
{
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000100 | iicaddr);
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000000 | regaddr);
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000000 | buffer[0] >> 8);
	XIic_WriteReg(baseaddr, XIIC_DTR_REG_OFFSET, 0x00000200 | buffer[0]);
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x0000000D); //送信開始
	//次に SR を読み、TX_FIFO が空（0000_00C0）かどうかを判定する。同様にタイムアウト機構を使い、タイムアウト時間は 10ms。
	int timeout = 0;
	while (timeout < 10)
	{
		if (XIic_ReadReg(baseaddr, XIIC_SR_REG_OFFSET) & 0x80)
			break;
		usleep(1000);
		timeout++;
	}
	if (timeout == 10)
		return -1;//IIC 送信タイムアウトで失敗
	// 終了後に FIFO をクリア
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000007);
	XIic_WriteReg(baseaddr, XIIC_CR_REG_OFFSET , 0x00000005);

	return 0;
}

int Single_Cell_Read(UINTPTR XPAR_AXI_IIC_0_BASEADDR,  u8 Device_ID, u8 regaddr,u16 *readback)
{
	readback[0] = 0;
	u16 Received_data[16];
	int i = 0;
	while((Xil_In32(XPAR_AXI_IIC_0_BASEADDR + XIIC_SR_REG_OFFSET) & 0x000000C4) != 0x000000c0) {}//Check that all FIFOs are empty and that the bus is not busy by reading the Status register
	Xil_Out32(XPAR_AXI_IIC_0_BASEADDR + XIIC_DTR_REG_OFFSET, (Device_ID<<1)|0x00000100);//Write 0x___ to the TX_FIFO (set the start bit, the device address, write access)
	Xil_Out32(XPAR_AXI_IIC_0_BASEADDR + XIIC_DTR_REG_OFFSET, regaddr);//Write 0x__ to the TX_FIFO (slave address for data)
	Xil_Out32(XPAR_AXI_IIC_0_BASEADDR + XIIC_DTR_REG_OFFSET, (Device_ID<<1)|0x00000101);//Write 0x___ to the TX_FIFO (set start bit, device address to 0x__, read access)
	Xil_Out32(XPAR_AXI_IIC_0_BASEADDR + XIIC_DTR_REG_OFFSET, 0x00000201);//Write 0x___ to the TX_FIFO (set stop bit, four bytes to be received by the AXI IIC)
	while((Xil_In32(XPAR_AXI_IIC_0_BASEADDR + XIIC_SR_REG_OFFSET) & 0x00000040) == 0x00000040) {}//Wait until the RX_FIFO is not empty.
	while((Xil_In32(XPAR_AXI_IIC_0_BASEADDR + XIIC_SR_REG_OFFSET) & 0x00000040) != 0x00000040){
		Received_data[i++] = Xil_In32(XPAR_AXI_IIC_0_BASEADDR + XIIC_DRR_REG_OFFSET);
	}
	readback[0] = Received_data[0] | Received_data[1] << 8;
	return 0;
}