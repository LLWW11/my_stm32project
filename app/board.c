#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include "AHT20.h"
#include "TFT_LCD.h"
#include "board.h"
#include "delay.h"
#include "esp_at.h"
#include "key.h"
#include "led.h"
#include "led_describe.h"
#include "tim.h"
#include "usart.h"

static struct led_desc led0 = {GPIOF, GPIO_Pin_9, Bit_RESET, Bit_SET};
static struct led_desc led1 = {GPIOF, GPIO_Pin_10, Bit_RESET, Bit_SET};

led_desc_t pled0 = &led0;
led_desc_t pled1 = &led1;

void board_low_level_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE); // led外设是PF9和PF10

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);  // USART1是PA9和PA10
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); // 串口1的时钟

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);  // USART3是PA9和PA10
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); // 串口3的时钟

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE); // key外设是PE2 3 4
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);  // TIM2时钟使能
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);  // TIM3时钟使能

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); // 系统时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);   // DMA时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // RTC

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSEConfig(RCC_LSE_ON);
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
        ;
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
}
void board_Init(void)
{
    // 各个外设模块初始化
    key_Init();
    usart_Init();
    // usart2_Init();

    USART_ReceiveData(USART2); // 清空上电时的串口接收寄存器杂乱数据
    TIM2_init();
    led_init(pled1);

    vTaskDelay(pdMS_TO_TICKS(10));

    // 上层模块初始化
    if (esp_at_init() == false)
#if ENABLE_DEBUG_PRINT
        printf("[ERR] ESP AT Init Failed!\r\n");
#endif

    if (esp_at_WiFi_Init() == false)
#if ENABLE_DEBUG_PRINT
        printf("[ERR] ESP WiFi Mode Init Failed!\r\n");
#endif

    TIM14_PWM_Init();
    led_init_PWM();
    // TFT_init(); //放在UI初始化哪里了
    AHT20_Init();
    // 这句话还是要的
    printf("[SYS] Build Date:%s %s\r\n", __DATE__, __TIME__);
}