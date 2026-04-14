#include "stm32f4xx.h"
#include "led_describe.h"
#include "led.h"
#include <stdlib.h>
#include <stdbool.h>

#define LED_PORT GPIOF
#define LED1_PIN GPIO_Pin_9	 // 开发版上这个是红灯
#define LED2_PIN GPIO_Pin_10 // 开发版上这个是绿灯

void led_all_off(void);

/*	通用的方法	*/
void LED_Init(void)
{
	extern void board_low_level_init(void);
	board_low_level_init();
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = LED1_PIN | LED2_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(LED_PORT, &GPIO_InitStructure);

	GPIO_SetBits(LED_PORT, LED1_PIN);
	GPIO_SetBits(LED_PORT, LED2_PIN);
}

void LED_ON(void)
{
	GPIO_ResetBits(LED_PORT, LED1_PIN);
	GPIO_SetBits(LED_PORT, LED2_PIN);
}

/*	面向对象的方法	*/
void led_init(led_desc_t led)
{
	extern void board_low_level_init(void);
	board_low_level_init();
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = led->Pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(led->Port, &GPIO_InitStructure);
	led_all_off();
}
void led_init_PWM(void)
{
	extern void board_low_level_init(void);
	board_low_level_init();
	GPIO_PinAFConfig(GPIOF, GPIO_PinSource9, GPIO_AF_TIM14);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
}
void led_set(led_desc_t led, bool onoff)
{
	GPIO_WriteBit(led->Port, led->Pin, onoff ? led->OnBit : led->OffBit);
}

void led_on(led_desc_t led)
{
	GPIO_ResetBits(led->Port, led->Pin);
}
void led_off(led_desc_t led)
{
	GPIO_SetBits(led->Port, led->Pin);
}
void led_all_off(void)
{
	GPIO_SetBits(GPIOF, LED1_PIN | LED2_PIN);
}
void led_reverse(led_desc_t led)
{
	if (GPIO_ReadOutputDataBit(led->Port, led->Pin) == Bit_RESET)
		GPIO_SetBits(led->Port, led->Pin);
	else
		GPIO_ResetBits(led->Port, led->Pin);
}
void led0_pwm(void)
{
	static int16_t led_pwm_val = 0;
	static int8_t dir = 1;
	dir == 1 ? led_pwm_val++ : led_pwm_val--;
	if (led_pwm_val >= 800)
	{
		led_pwm_val = 800;
		dir = -1; // 开始变暗
	}
	if (led_pwm_val <= 0)
	{
		led_pwm_val = 0;
		dir = 1; // 开始变亮
	}
	TIM_SetCompare1(TIM14, led_pwm_val);
}