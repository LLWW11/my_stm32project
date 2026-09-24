#include "stm32f4xx.h"
#include "boot_UART.h"

/** 初始化 USART1 的 PA9 发送引脚，115200、8N1。 */
void boot_uart1_init(void)
{
    GPIO_InitTypeDef gpio_init;
    USART_InitTypeDef usart_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    // 将 PA9 连接到 USART1，串口仅用于输出日志。
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);

    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio_init);

    USART_StructInit(&usart_init);
    usart_init.USART_BaudRate = 115200;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Tx;
    USART_Init(USART1, &usart_init);
    USART_Cmd(USART1, ENABLE);
}


/** 阻塞发送 USART1 的一个字节。 */
void boot_uart1_send_byte(uint8_t data)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    USART_SendData(USART1, data);
}

/** 发送一个以空字符结尾的字符串，并等待末字节完成。 */
void boot_uart1_send_string(const char *text)
{
    while (*text != '\0')
    {
        boot_uart1_send_byte((uint8_t)*text);
        text++;
    }

    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

/** 以十六进制形式发送 32 位数据。 */
void boot_uart1_send_hex32(uint32_t value)
{
    static const char hex_table[] = "0123456789ABCDEF";
    int32_t shift;

    boot_uart1_send_string("0x");

    for (shift = 28; shift >= 0; shift -= 4)
    {
        uint8_t index = (uint8_t)((value >> shift) & 0x0FU);
        boot_uart1_send_byte((uint8_t)hex_table[index]);
    }
}
