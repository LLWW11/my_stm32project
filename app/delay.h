#ifndef __DELAY_H
#define __DELAY_H
#include <stdint.h>

typedef void (*cpu_periodic_callback_t)(void);

void Systick_init(void);
uint64_t cpu_get_ms(void); // 获取系统当前运行的ms数
uint64_t cpu_get_us(void);
void SysTick_delay_us(uint32_t us);
void delay_ms(uint32_t ms);
void delay_s(uint32_t s);
void cpu_register_periodic_callback(cpu_periodic_callback_t callback);
// void SysTick_delay_us(uint32_t us);
// void delay_ms(uint16_t ms);
// void delay_s(uint16_t s);
#endif /*__LED_H*/
