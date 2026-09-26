#ifndef WEATHERCLOCK_BOOT_UART_H
#define WEATHERCLOCK_BOOT_UART_H
#include "stm32f4xx.h"

/** 初始化仅用于日志输出的 USART1。 */
void boot_uart1_init(void);
/** 阻塞发送一个字节。 */
void boot_uart1_send_byte(uint8_t data);
/** 阻塞发送以零结尾的字符串。 */
void boot_uart1_send_string(const char *text);
/** 输出一个 32 位十六进制整数。 */
void boot_uart1_send_hex32(uint32_t value);
#endif /* WEATHERCLOCK_BOOT_UART_H */
