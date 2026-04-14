#ifndef __IIC_H
#define __IIC_H
#include "stm32f4xx.h"
#include "stdbool.h"
void IIC2_Init();
// static bool IIC2_SentData(uint8_t IIC_SendData, bool isAddr);
// static bool IIC2_ReceiveData(uint8_t Addr, uint8_t *dat, uint8_t len);
#endif /*__IIC_H*/
