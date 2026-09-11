#ifndef __DMA_H
#define __DMA_H
#include <stdbool.h>
void myDMA_Init(uint32_t memaddr, uint32_t dataLength, bool meminc);
void SPI_DMA_init(void);
void ESP_DMA_Init(void);
#endif /*__DMA_H*/
