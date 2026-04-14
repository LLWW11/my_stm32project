#ifndef __TIM_H
#define __TIM_H

extern uint16_t cnt;
extern uint8_t time_1s_flag;
extern volatile uint16_t esp_timeout;
void TIM2_init(void);
void TIM14_PWM_Init(void);
// void SysTick_Exti_Init(void);

#endif /*__TIM_H*/
