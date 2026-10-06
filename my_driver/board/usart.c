#include "stm32f4xx.h"
#include <stdio.h>
#include <stdbool.h>
#include "tim.h"
#include "usart.h"
// USART1用于输出调试信息
extern RingBuffer_t rx_buffer = {.head = 0, .tail = 0};
void usart_Init(void)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; //
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No; // 不设置校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    // USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
    USART_Cmd(USART1, ENABLE);
}
void usart1_SendByte(uint8_t data)
{
    USART_ClearFlag(USART1, USART_FLAG_TC);
    USART_SendData(USART1, data);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
        ;
}
void usart_sendString(uint8_t *str) // 发送字符串
{
    uint8_t i = 0;
    while (str[i] != '\0')
    {
        usart1_SendByte(str[i]);
        i++;
    }
}
int fputc(int c, FILE *f)
{
    //(void)stream;
    USART_ClearFlag(USART1, USART_FLAG_TC);
    USART_SendData(USART1, c);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
        ;
    return c;
}
uint16_t usart_ReceiveString(void)
{
    uint16_t readData;
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)
        readData = USART_ReceiveData(USART1);
    // USART_ClearFlag(USART1, USART_FLAG_RXNE);
    return readData;
}
uint8_t usart_ReceiveByte_NonBlock(uint8_t *rx_data)
{
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)
    {
        *rx_data = (uint8_t)USART_ReceiveData(USART1);
        return 1; // 返回 1 表示成功
    }
    else
        return 0;
}

void usart2_Init(void) // usart2用于和esp32通信
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; //
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No; // esp32c3默认配置
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    USART_Cmd(USART2, ENABLE);
}
/*
void clear_usart_buffer(void)
{
    rx_buffer.head = 0;
    rx_buffer.tail = 0;
}

void ring_buffer_push(uint8_t data)
{
    uint16_t next_head = (rx_buffer.head + 1) % RING_BUFFER_SIZE;
    if (next_head != rx_buffer.tail)
    { 
        rx_buffer.buffer[rx_buffer.head] = data;
        rx_buffer.head = next_head;
    }
}
bool ring_buffer_pop(uint8_t *data)
{
    if (rx_buffer.head == rx_buffer.tail)
    {
        return false; 
    }
    *data = rx_buffer.buffer[rx_buffer.tail];
    rx_buffer.tail = (rx_buffer.tail + 1) % RING_BUFFER_SIZE;
    return true;
}
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t rx_data = USART_ReceiveData(USART2);
        ring_buffer_push(rx_data);
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

uint16_t usart2_receiveString(char *buffer, uint16_t bufferSize, uint32_t timeout) // usart2接收字符串,超时时间为ms
{
    uint16_t index = 0;
    uint8_t rxData;
    esp_timeout = 0;

    while (index < bufferSize - 1)
    {
        if (ring_buffer_pop(&rxData))
        {
            buffer[index++] = rxData;
            esp_timeout = 0; // 只要读到数据，就刷新超时计数器
        }
        else
        {
            // 缓冲区空了，说明此时没有新数据，判断是否整体超时
            if (esp_timeout >= timeout)
            {
                esp_timeout = 0;
                break;
            }
        }
    }
    buffer[index] = '\0';
    return index; // 返回接收到的数据长度
}
    */