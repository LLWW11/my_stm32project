#ifndef __KEY_H
#define __KEY_H
#include "stm32f4xx.h"

#define KEY_PORT GPIOE
#define KEY0 GPIO_Pin_4
#define KEY1 GPIO_Pin_3
#define KEY2 GPIO_Pin_2

typedef struct
{
    GPIO_TypeDef *Port;  // GPIO端口
    uint16_t Pin;        // GPIO引脚
    uint8_t DebounceCnt; // 消抖计数器,连续DebounceCnt次按下或者松开才算有效
    uint8_t State;       // 当前按键状态：0-松开，1-按下
    uint8_t PressFlag;   // 按下标志位，1表示发生了一次按下
    uint8_t ReleaseFlag; // 松开标志位，1表示发生了一次松开
} Key_Dev_t;

extern Key_Dev_t KeyList[];

void key_Init(void);
void key_Tick(void);

#endif /*__TIM_H*/
