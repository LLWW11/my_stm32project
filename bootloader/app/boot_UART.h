#ifndef __BOOT_UART_H
#define __BOOT_UART_H
#include "stm32f4xx.h"
void boot_uart1_init(void);
void boot_uart1_send_byte(uint8_t data);
void boot_uart1_send_string(const char *text);
void boot_uart1_send_hex32(uint32_t value);
#endif // !__BOOT_UART_H
