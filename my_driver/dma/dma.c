#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>
// SPI用DMA
void myDMA_Init(uint32_t memaddr, uint32_t dataLength, bool meminc)
{
    DMA_InitTypeDef dma_initstructure;
    DMA_StructInit(&dma_initstructure);

    dma_initstructure.DMA_BufferSize = dataLength;
    dma_initstructure.DMA_Memory0BaseAddr = memaddr;
    dma_initstructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI1->DR);
    dma_initstructure.DMA_Channel = DMA_Channel_3; // DMA2映射的通道3发送端
    dma_initstructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    dma_initstructure.DMA_FIFOMode = DMA_FIFOMode_Disable;

    dma_initstructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma_initstructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma_initstructure.DMA_MemoryInc =
        meminc ? DMA_MemoryInc_Enable : DMA_MemoryInc_Disable;
    // 刷纯色背景是一个字节，其他是两个字节
    dma_initstructure.DMA_Mode = DMA_Mode_Normal;
    dma_initstructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_initstructure.DMA_Priority = DMA_Priority_Medium;

    DMA_Init(DMA2_Stream5, &dma_initstructure);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA2_Stream5, ENABLE);
    while (DMA_GetFlagStatus(DMA2_Stream5, DMA_FLAG_TCIF5) == RESET)
        ;
    DMA_ClearFlag(DMA2_Stream5, DMA_FLAG_TCIF5);
    while (SPI_GetFlagStatus(SPI1, SPI_FLAG_BSY) != RESET)
        ;
    NVIC_InitTypeDef nvic_initstructure;
    nvic_initstructure.NVIC_IRQChannel = DMA2_Stream5_IRQn;
    nvic_initstructure.NVIC_IRQChannelCmd = ENABLE;
    nvic_initstructure.NVIC_IRQChannelPreemptionPriority = 5;
    nvic_initstructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&nvic_initstructure);
    NVIC_SetPriority(DMA2_Stream5_IRQn, 5);
}

void SPI_DMA_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_3;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DR;
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

    DMA_Init(DMA2_Stream5, &DMA_InitStruct);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA2_Stream5, ENABLE);
    DMA_ITConfig(DMA2_Stream5, DMA_IT_TC, ENABLE);
    NVIC_InitTypeDef nvic_initstructure;
    nvic_initstructure.NVIC_IRQChannel = DMA2_Stream5_IRQn;
    nvic_initstructure.NVIC_IRQChannelCmd = ENABLE;
    nvic_initstructure.NVIC_IRQChannelPreemptionPriority = 6;
    nvic_initstructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&nvic_initstructure);
    NVIC_SetPriority(DMA2_Stream5_IRQn, 6);
}

void ESP_DMA_Init(void)
{


    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

    DMA_Init(DMA1_Stream6, &DMA_InitStruct);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
    // DMA_Cmd(DMA1_Stream6, ENABLE);
}