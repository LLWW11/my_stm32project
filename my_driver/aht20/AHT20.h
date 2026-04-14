#ifndef __AHT20_H
#define __AHT20_H
#include <stdbool.h>
#include "stm32f4xx.h"
bool AHT20_Init(void);
static bool AHT20_write(uint8_t data[], uint32_t len);
static bool AHT20_read(uint8_t data[], uint32_t len);
bool aht20_wait_for_measurement(void);
bool aht20_read_measurement(float *temperature, float *humidity);
bool aht20_start_measurement(void);
#endif /*__AHT20_H*/ 