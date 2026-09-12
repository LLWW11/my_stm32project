#include "stm32f4xx.h"
#define SPI_CS GPIO_Pin_4 // CS信号线
#define SPI_SCK GPIO_Pin_5
#define SPI_MISO GPIO_Pin_6 // 这个用不到
#define SPI_MOSI GPIO_Pin_7
/*
硬件连接：
SDA：PA7   	MOSI数据线
SCL：PA5	SCK时钟线
RST：PD7	复位数据线,在正常操作过程中，保持拉高
DC： PD6	数据/控制线：1表示显示数据，0表示写入寄存器
CS： PA4	片选，低电平有效
BL： PD13	背光控制pin，当被拉高时打开背光，当被拉低时关闭背光
*/
void SPI1_Init(void)
{
    //
    GPIO_InitTypeDef GPIOA_InitStructure;
    GPIO_StructInit(&GPIOA_InitStructure);
    GPIOA_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIOA_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIOA_InitStructure.GPIO_Pin = SPI_SCK | SPI_MISO | SPI_MOSI;
    GPIOA_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIOA_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIOA_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);

    GPIOA_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIOA_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOA, &GPIOA_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4); // 初始化拉高 CS
    // 控制引脚
    GPIO_InitTypeDef GPIOD_InitStructure;
    GPIOD_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIOD_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIOD_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_1;
    GPIOD_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIOD_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_Init(GPIOD, &GPIOD_InitStructure);
    // SPI初始化
    SPI_InitTypeDef SPI1_InitStruct;
    SPI_StructInit(&SPI1_InitStruct);
    SPI1_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI1_InitStruct.SPI_CPHA = SPI_CPHA_1Edge; // 第一个上升沿采样
    SPI1_InitStruct.SPI_CPOL = SPI_CPOL_Low;   // 空闲时SPI时钟为低电平
    SPI1_InitStruct.SPI_DataSize = SPI_DataSize_8b;
    SPI1_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI1_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI1_InitStruct.SPI_Mode = SPI_Mode_Master;
    SPI1_InitStruct.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(SPI1, &SPI1_InitStruct);
    SPI_Cmd(SPI1, ENABLE);
}
void SPI1_SendByte(uint8_t dat)
{
    while (!SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE))
        ;
    SPI_SendData(SPI1, dat);
    while (SPI_GetFlagStatus(SPI1, SPI_FLAG_BSY) == SET)
        ;
}


/*
SPI3用于读写W25Q128
硬件连接：
MOSI ：PB5
MISO ：PB4
SCLK ：PB3
CS   ：PB14
*/

void SPI3_Init(void)
{

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);

    GPIO_InitTypeDef GPIOB_InitStructure;
    GPIO_StructInit(&GPIOB_InitStructure);
    GPIOB_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIOB_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIOB_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIOB_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIOB_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIOB_InitStructure);

    // 引脚复用功能 AF6
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_SPI3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_SPI3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_SPI3);

    GPIOB_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIOB_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIOB_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIOB_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIOB_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIOB_InitStructure);

    // 初始化拉高 CS
    GPIO_SetBits(GPIOB, GPIO_Pin_14);

    SPI_InitTypeDef SPI3_InitStruct;
    SPI_StructInit(&SPI3_InitStruct);
    SPI3_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex; // 
    SPI3_InitStruct.SPI_Mode = SPI_Mode_Master;                      // 
    SPI3_InitStruct.SPI_DataSize = SPI_DataSize_8b;                  // 
    SPI3_InitStruct.SPI_CPOL = SPI_CPOL_High;                        //
    SPI3_InitStruct.SPI_CPHA = SPI_CPHA_2Edge;
    SPI3_InitStruct.SPI_NSS = SPI_NSS_Soft;                         
    SPI3_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; 
    SPI3_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;                
    SPI3_InitStruct.SPI_CRCPolynomial = 7;
    SPI_Init(SPI3, &SPI3_InitStruct);

    SPI_Cmd(SPI3, ENABLE); // 使能 SPI3
}

void SPI3_SendByte(uint8_t dat)
{
    while (!SPI_GetFlagStatus(SPI3, SPI_FLAG_TXE))
        ;
    SPI_SendData(SPI3, dat);
    while (SPI_GetFlagStatus(SPI3, SPI_FLAG_BSY) == SET)
        ;
}

// void SPI1_Wait_Busy(void)
// {
//     while (!SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE));
//     while (SPI_GetFlagStatus(SPI1, SPI_FLAG_BSY) == SET);
// }