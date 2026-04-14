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

void board_low_level_init(void) {
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
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);    // RTC
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSEConfig(RCC_LSE_ON);
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
        ;
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
}
void board_Init(void) {
    Systick_init();
    // 各个外设模块初始化
    key_Init();
    usart_Init();
    usart2_Init();
    USART_ReceiveData(USART2); // 清空上电时的串口接收寄存器杂乱数据
    TIM2_init();
    led_init(pled1);

    // ESP32与串口清理
    usart2_sendString("AT\r\n"); // 清理f407串口缓冲区
    delay_ms(10);

    // 上层模块初始化
    esp_at_WiFi_Init();
    TIM14_PWM_Init();
    led_init_PWM();
    TFT_init();
    AHT20_Init();

    printf("[SYS] Build Date:%s %s\r\n", __DATE__, __TIME__);
}