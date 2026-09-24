#include "stm32f4xx.h"
#include "boot_UART.h"

/** 初始化 USART1 的 PA9/PA10、115200、8N1。 */
void boot_uart1_init(void)
{
    GPIO_InitTypeDef gpio_init;
    USART_InitTypeDef usart_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    //将 PA9、PA10 连接到 USART1 的复用功能
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
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
    usart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
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

/** 非阻塞读取 USART1 的一个字节，同时清除接收溢出状态。 */
uint8_t boot_uart1_try_read(uint8_t *data)
{
    if (data == 0)
        return 0U;

    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)
    {
        *data = (uint8_t)USART_ReceiveData(USART1);
        return 1U;
    }

    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET)
    {
        (void)USART_ReceiveData(USART1);
    }

    return 0U;
}

/** 使用 SysTick 轮询 USART1，超过 timeout_ms 毫秒时返回失败。 */
uint8_t boot_uart1_read_timeout(uint8_t *data, uint32_t timeout_ms)
{
    uint32_t reload;
    uint32_t elapsed;

    if ((data == 0) || (timeout_ms == 0U))
        return 0U;

    reload = SystemCoreClock / 1000U;
    if ((reload == 0U) || ((reload - 1U) > SysTick_LOAD_RELOAD_Msk))
        return 0U;

    SysTick->LOAD = reload - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    elapsed = 0U;
    while (elapsed < timeout_ms)
    {
        if (boot_uart1_try_read(data) != 0U)
        {
            SysTick->CTRL = 0U;
            return 1U;
        }

        if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U)
            elapsed++;
    }

    SysTick->CTRL = 0U;
    return 0U;
}
