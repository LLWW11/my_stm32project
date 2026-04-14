#ifndef __LED_DESCRIBE_H
#define __LED_DESCRIBE_H

#include "stm32f4xx.h"

struct led_desc
{
	GPIO_TypeDef *Port; /*端口号，第几个IO口*/
	uint32_t Pin;
	BitAction OnBit;
	BitAction OffBit;
};

typedef struct led_desc *led_desc_t;

#endif /*__LED_H*/
