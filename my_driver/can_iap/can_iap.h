#ifndef __CAN_IAP_H
#define __CAN_IAP_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "stm32f4xx_can.h"
//经典CAN，11bit ID，500kb/s
//任务内轮询FIFO

/**
 * @brief 硬件初始化
 * @param loopback true表示内部回环测试，false表示正常总线测试
 * @return true 初始化成功
 */
bool can_test_init(bool loopback);
/**
 * @brief 从CAN1的FIFO0轮询接收一帧标注数据
 * @param rx 接收缓冲区 
 * @param timeout_ticks 最长等待数
 * @return true 收到有效的标准数据帧
 * @return false 
 */
bool can_test_receive(CanRxMsg *rx, TickType_t timeout_ticks);

/**
 * @brief 发送一 CAN 数据帧
 * @param id 11 位标准 ID，范围为 0～0x7FF
 * @param data 数据缓冲区；len 为 0 时允许为 NULL
 * @param len 数据长度，范围为 0～8
 * @param timeout_ticks 等待发送结果的最长 tick 数
 * @return true 表示控制器报告发送成功
 */
bool can_test_send(uint16_t id, const uint8_t *data,
                   uint8_t len, TickType_t timeout_ticks);

#endif // __CAN_IAP_H
