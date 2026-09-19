#ifndef __SPI_H
#define __SPI_H
#include "stm32f4xx.h"
void SPI1_Init(void);
void SPI3_Init(void);
void SPI1_SendByte(uint8_t dat);
// 通过 SPI3 全双工发送一个字节，并返回同时接收到的字节
uint8_t SPI3_ReadWriteByte(uint8_t dat);

#endif /*__SPI_H*/
