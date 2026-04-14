#include "stm32f4xx.h"
#include "stdint.h"
#include "delay.h"
// 168 000 000Hz
#define TICKS_PER_MS SystemCoreClock / 1000		   // 每1ms计数168*1k次
#define TICKS_PER_US SystemCoreClock / 1000 / 1000 // 每1us计数168次

static volatile uint64_t cpu_tick_count;
static cpu_periodic_callback_t periodic_callback;

void Systick_init(void)
{
	SysTick->LOAD = TICKS_PER_MS;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}
uint64_t cpu_now(void) // 获取系统当前运行的systick数
{
	uint64_t now, last_count;
	do
	{
		last_count = cpu_tick_count;
		now = cpu_tick_count + SysTick->LOAD - SysTick->VAL;
	} while (last_count != cpu_tick_count);
	return now;
}
uint64_t cpu_get_ms(void) // 获取系统当前运行的ms数
{
	return cpu_now() / TICKS_PER_MS;
}
uint64_t cpu_get_us(void)
{
	return cpu_now() / TICKS_PER_US;
}
void SysTick_delay_us(uint32_t us)
{
	uint64_t now = cpu_now();
	while (cpu_now() - now < (uint64_t)us * TICKS_PER_US)
		;
}
void delay_ms(uint32_t ms)
{
	uint64_t now = cpu_now();
	while (cpu_now() - now < (uint64_t)ms * TICKS_PER_MS)
		;
}
void delay_s(uint32_t s)
{
	for (uint32_t i = s; i > 0; i--)
		delay_ms(1);
}
void cpu_register_periodic_callback(cpu_periodic_callback_t callback)
{
	periodic_callback = callback;
}
void SysTick_Handler(void)
{
	cpu_tick_count += TICKS_PER_MS;
	if (periodic_callback)
		periodic_callback();
}

// void SysTick_delay_us(uint32_t us)
// {
// 	// systick只能向下计数
// 	SysTick->LOAD = us * (SystemCoreClock / 1000 / 1000) - 1; // 设置计数值
// 	SysTick->VAL = 0;
// 	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // 设置系统频率并使能
// 	// Returns 1 if timer counted to 0 since last time this was read.
// 	// 读取CTRL硬件会自动清零 COUNTFLAG 位
// 	while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
// 		;
// 	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
// }

// void delay_ms(uint16_t ms)
// {
// 	for (uint16_t i = ms; i > 0; i--)
// 	{
// 		SysTick_delay_us(1000);
// 	}
// }

// void delay_s(uint16_t s)
// {
// 	for (uint16_t i = s; i > 0; i--)
// 	{
// 		delay_ms(1000);
// 	}
// }
