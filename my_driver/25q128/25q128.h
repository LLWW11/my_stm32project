#ifndef __25Q128_H
#define __25Q128_H

#include <stdint.h>

#define W25Q128_JEDEC_ID 0x00EF4018UL
#define W25Q128_SECTOR_SIZE 4096UL
#define W25Q128_PAGE_SIZE 256UL

void norflash_Init(void);// 初始化 W25Q128 使用的 SPI3 外设
uint8_t W25Q_ReadSR1(void);// 读取 W25Q128 状态寄存器1
void W25Q_WriteEnable(void);//写使能
void W25Q_WaitBusy(void);//等待繁忙
uint32_t W25Q_ReadID(void);//读取芯片ID
void W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);// 从指定地址读取连续数据
void W25Q_SectorErase(uint32_t addr);//擦除指定地址所在的 4 KiB 扇区
void W25Q_PageWrite(uint32_t addr, const uint8_t *buf, uint32_t len);// 向单个页内写入不超过 256 字节的数据

#endif // !__25Q128_H
