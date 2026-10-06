#ifndef TEST_AT24C02_H
#define TEST_AT24C02_H
#include <stdint.h>

/* 仅替代主机测试中的 EEPROM 接口，不包含 STM32 I2C 驱动。 */
typedef enum { AT24C02_OK = 0, AT24C02_ERROR_NACK } at24c02_status_t;
/** 返回模拟 EEPROM 的可用状态。 */
at24c02_status_t AT24C02_Init(void);
/** 从模拟 EEPROM 有界读取数据。 */
at24c02_status_t AT24C02_ReadBuffer(uint16_t address, uint8_t *data, uint32_t length);
/** 向模拟 EEPROM 有界写入，可注入写入失败。 */
at24c02_status_t AT24C02_WriteBuffer(uint16_t address, const uint8_t *data, uint32_t length);
#endif
