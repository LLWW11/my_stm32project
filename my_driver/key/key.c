#include "stm32f4xx.h"
#include "key.h"

Key_Dev_t KeyList[] = {
    {KEY_PORT, KEY0, 0, 0, 0, 0}, // KEY0
    {KEY_PORT, KEY1, 0, 0, 0, 0}, // KEY1
    {KEY_PORT, KEY2, 0, 0, 0, 0}  // KEY2
};

#define KEY_NUM (sizeof(KeyList) / sizeof(KeyList[0]))
#define DEBOUNCE_TICKS 8 // 检测8次

void key_Init()
{
    extern void board_low_level_init(void);
    board_low_level_init();

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = KEY0 | KEY1 | KEY2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; // 另一端接地的
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(KEY_PORT, &GPIO_InitStructure);
}

void key_Tick(void) // 按下和松开都检测
{
    uint8_t i;
    for (i = 0; i < KEY_NUM; i++)
    {
        if (GPIO_ReadInputDataBit(KeyList[i].Port, KeyList[i].Pin) == Bit_RESET)
        {
            if (KeyList[i].State == 0) //
            {
                KeyList[i].DebounceCnt++;
                if (KeyList[i].DebounceCnt >= DEBOUNCE_TICKS) // 消抖确认
                {
                    KeyList[i].State = 1; // 确认为按下状态
                    KeyList[i].PressFlag = 1;
                }
            }
        }
        else
        {
            if (KeyList[i].State == 1)
            {
                KeyList[i].DebounceCnt++;
                if (KeyList[i].DebounceCnt >= DEBOUNCE_TICKS)
                {
                    KeyList[i].State = 0; // 确认为松开状态
                    KeyList[i].ReleaseFlag = 1;
                }
            }
            else
                KeyList[i].DebounceCnt = 0;
        }
    }
}
