#include "key.h"
#include "board.h"
#include "led.h"
#include "esp_at.h"

uint16_t cnt = 0;
volatile uint8_t time_1s_flag = 0;
volatile uint16_t esp_timeout = 0;

void TIM2_init(void)
{
    // 定时1ms
    TIM_TimeBaseInitTypeDef TIM2_TimeBaseStructure;
    TIM_TimeBaseStructInit(&TIM2_TimeBaseStructure);
    // 直接获取APB2总线的时钟频率
    RCC_ClocksTypeDef RCC_ClockStruct;
    RCC_GetClocksFreq(&RCC_ClockStruct);
    uint32_t APB2_timeClock = RCC_ClockStruct.PCLK1_Frequency * 2;
    TIM2_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV2;
    TIM2_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM2_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM2_TimeBaseStructure.TIM_Prescaler = APB2_timeClock / 1000000 - 1;
    TIM2_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM2_TimeBaseStructure);
    TIM_Cmd(TIM2, ENABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}
void TIM14_PWM_Init(void)
{
    RCC_ClocksTypeDef RCC_ClockStruct;
    RCC_GetClocksFreq(&RCC_ClockStruct);
    uint32_t APB2_timeClock = RCC_ClockStruct.PCLK1_Frequency * 2;

    TIM_TimeBaseInitTypeDef TIM14_TimeBaseStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, ENABLE);
    // TIM14时钟84MHz,定时1ms
    TIM14_TimeBaseStructure.TIM_Prescaler = APB2_timeClock / 1000000 - 1;
    TIM14_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数
    TIM14_TimeBaseStructure.TIM_Period = 1000 - 1;                // 自动重装载值
    TIM14_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM14_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM14, &TIM14_TimeBaseStructure);

    TIM_OCInitTypeDef TIM14_OCInitStructure;
    TIM14_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             // PWM模式1
    TIM14_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // 输出使能
    TIM14_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM14_OCInitStructure.TIM_Pulse = 0;
    TIM_OC1Init(TIM14, &TIM14_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM14, ENABLE);
    TIM_OC1PreloadConfig(TIM14, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM14, ENABLE);

    TIM_Cmd(TIM14, ENABLE);
}

void TIM2_IRQHandler()
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        esp_timeout++;
        key_Tick();
        if (cnt % 2 == 0)
            led0_pwm();
        if (++cnt >= 2000)
        {
            cnt = 0;
            time_1s_flag = 1;
        }
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}