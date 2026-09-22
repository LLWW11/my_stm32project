#ifndef WEATHERCLOCK_SPI_H
#define WEATHERCLOCK_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include "stm32f4xx.h"

/**
 * @brief 初始化连接 TFT 的 SPI1 外设。
 */
void SPI1_Init(void);

/**
 * @brief 初始化连接 W25Q128 的 SPI3 外设。
 */
void SPI3_Init(void);

/**
 * @brief 通过 SPI1 发送一个字节。
 * @param dat 待发送的数据。
 */
void SPI1_SendByte(uint8_t dat);

/**
 * @brief 通过 SPI3 全双工交换一个字节。
 * @param transmit_data 待发送的数据。
 * @param receive_data 用于接收数据的指针，可为 NULL。
 * @param timeout_count 等待 SPI 状态标志的最大轮询次数。
 * @return 通信完成返回 true，SPI 状态标志超时返回 false。
 */
bool SPI3_TransferByte(uint8_t transmit_data,
                       uint8_t *receive_data,
                       uint32_t timeout_count);

/**
 * @brief 等待 SPI3 完成当前字节传输。
 * @param timeout_count 等待 BSY 清零的最大轮询次数。
 * @return SPI3 空闲返回 true，超时返回 false。
 */
bool SPI3_WaitIdle(uint32_t timeout_count);

#endif /* WEATHERCLOCK_SPI_H */
