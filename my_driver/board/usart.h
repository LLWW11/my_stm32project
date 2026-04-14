#ifndef __USART_H
#define __USART_H
#include <stdio.h>

#define RING_BUFFER_SIZE 1024
typedef struct
{
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head; // 写指针（门卫用的）
    volatile uint16_t tail; // 读指针（你用的）
} RingBuffer_t;

void usart_Init(void);
void usart1_SendByte(uint8_t data);
void usart_sendString(uint8_t *str);
uint16_t usart_ReceiveString(void);
uint8_t usart_ReceiveByte_NonBlock(uint8_t *rx_data);

void usart2_Init(void);
void clear_usart_buffer();
bool ring_buffer_pop(uint8_t *data);
void ring_buffer_push(uint8_t data);
uint16_t usart2_receiveString(char *buffer, uint16_t bufferSize, uint32_t timeout);
// int fputc(int c,FILE *stream);
#endif /*__USART_H*/
