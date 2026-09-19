#include "25q128.h"
#include "SPI.h"

#define F_CS_L   GPIO_ResetBits(GPIOB, GPIO_Pin_14)
#define F_CS_H  GPIO_SetBits(GPIOB, GPIO_Pin_14)

void norflash_Init(void)
{
    SPI3_Init();
                            
}

uint8_t W25Q_ReadSR1(void)
{
    uint8_t sr;
    F_CS_L;
    SPI3_ReadWriteByte(0x05);       
    sr = SPI3_ReadWriteByte(0xFF);    
    F_CS_H;
    return sr;
}

void W25Q_WriteEnable(void)
{
    F_CS_L;
    SPI3_ReadWriteByte(0x06);
    F_CS_H;
}

void W25Q_WaitBusy(void)
{
    uint32_t t = 0;
    while ((W25Q_ReadSR1() & 0x01) != 0)
        if (++t > 0x00400000) break;  
}

uint32_t W25Q_ReadID(void) //读芯片ID
{
    uint32_t id = 0;
    F_CS_L;
    SPI3_ReadWriteByte(0x9F);
    id |= (uint32_t)SPI3_ReadWriteByte(0xFF) << 16;   //厂商
    id |= (uint32_t)SPI3_ReadWriteByte(0xFF) << 8;    //类型
    id |= (uint32_t)SPI3_ReadWriteByte(0xFF);         //容量
    F_CS_H;
    return id;
}
//读不需要跨页
void W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    F_CS_L;
    SPI3_ReadWriteByte(0x03);
    SPI3_ReadWriteByte((uint8_t)(addr >> 16));  
    SPI3_ReadWriteByte((uint8_t)(addr >> 8));
    SPI3_ReadWriteByte((uint8_t)addr);
    while (len--)
        *buf++ = SPI3_ReadWriteByte(0xFF);          
    F_CS_H;
}
//按扇区擦除
void W25Q_SectorErase(uint32_t addr)
{
    W25Q_WriteEnable();
    F_CS_L;
    SPI3_ReadWriteByte(0x20);
    SPI3_ReadWriteByte((uint8_t)(addr >> 16));
    SPI3_ReadWriteByte((uint8_t)(addr >> 8));
    SPI3_ReadWriteByte((uint8_t)addr);
    F_CS_H;                
    W25Q_WaitBusy();         
}

//向addr写buf，长度<256
void W25Q_PageWrite(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    W25Q_WriteEnable();     
    F_CS_L;
    SPI3_ReadWriteByte(0x02);
    SPI3_ReadWriteByte((uint8_t)(addr >> 16));
    SPI3_ReadWriteByte((uint8_t)(addr >> 8));
    SPI3_ReadWriteByte((uint8_t)addr);
    while (len--)
        SPI3_ReadWriteByte(*buf++);
    F_CS_H;               
    W25Q_WaitBusy();        
}
