#ifndef __SPI_H
#define __SPI_H
#include "stm32f4xx.h"
void SPI1_Init(void);
void SPI3_Init(void);
void SPI1_SendByte(uint8_t dat);
// void SPI1_Wait_Busy(void);
#endif /*__SPI_H*/
