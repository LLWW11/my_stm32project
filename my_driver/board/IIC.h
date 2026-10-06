#ifndef __IIC_H
#define __IIC_H

#include "stm32f4xx.h"
#include "stdbool.h"

void IIC2_Init();
// static bool IIC2_SentData(uint8_t IIC_SendData, bool isAddr);
// static bool IIC2_ReceiveData(uint8_t Addr, uint8_t *dat, uint8_t len);

void IIC1_Init(void);

/** 总线卡住的自救，9个 SCL 脉冲 + 一个 STOP返回 true 表示总线已回到空闲 */
bool IIC1_BusRecover(void);

/** 探测器件在不在 / EEPROM 写周期是否结束 */
bool IIC1_Probe(uint8_t dev_addr_7bit);

/** START -> 地址+W -> data... -> STOP */
bool IIC1_WriteBytes(uint8_t dev_addr_7bit,
                     const uint8_t *data,
                     uint32_t length);

/** START -> 地址+R -> data... -> NACK -> STOP */
bool IIC1_ReadBytes(uint8_t dev_addr_7bit,
                    uint8_t *data,
                    uint32_t length);


bool IIC1_WriteReadBytes(uint8_t dev_addr_7bit,
                         const uint8_t *tx,
                         uint32_t tx_length,
                         uint8_t *rx,
                         uint32_t rx_length);

#endif /*__IIC_H*/
