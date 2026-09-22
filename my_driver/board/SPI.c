#include "SPI.h"
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
/**
 * @brief 初始化连接 TFT 的 SPI1 外设。
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
/**
 * @brief 通过 SPI1 发送一个字节。
 * @param dat 待发送的数据。
 */
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

/**
 * @brief 初始化连接 W25Q128 的 SPI3 外设。
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

    /* 软件管理 NSS 时，必须将内部 NSS 置高以保持主机模式。 */
    SPI_NSSInternalSoftwareConfig(SPI3, SPI_NSSInternalSoft_Set);
    SPI_Cmd(SPI3, ENABLE); // 使能 SPI3
}

/**
 * @brief 等待 SPI3 指定状态标志变为目标状态。
 * @param flag 需要检查的 SPI 状态标志。
 * @param expected_status 期望的标志状态。
 * @param timeout_count 最大轮询次数。
 * @return 达到目标状态返回 true，超时返回 false。
 */
static bool SPI3_WaitForFlag(uint16_t flag,
                             FlagStatus expected_status,
                             uint32_t timeout_count)
{
    while (SPI_GetFlagStatus(SPI3, flag) != expected_status)
    {
        if (timeout_count == 0U)
        {
            return false;
        }

        timeout_count--;
    }

    return true;
}

/**
 * @brief 通过 SPI3 全双工交换一个字节。
 * @param transmit_data 待发送的数据。
 * @param receive_data 用于接收数据的指针，可为 NULL。
 * @param timeout_count 等待 SPI 状态标志的最大轮询次数。
 * @return 通信完成返回 true，SPI 状态标志超时返回 false。
 */
bool SPI3_TransferByte(uint8_t transmit_data,
                       uint8_t *receive_data,
                       uint32_t timeout_count)
{
    uint8_t received_data;

    if (!SPI3_WaitForFlag(SPI_FLAG_TXE, SET, timeout_count))
    {
        return false;
    }

    SPI_SendData(SPI3, transmit_data);

    if (!SPI3_WaitForFlag(SPI_FLAG_RXNE, SET, timeout_count))
    {
        return false;
    }

    received_data = (uint8_t)SPI_ReceiveData(SPI3);
    if (receive_data != NULL)
    {
        *receive_data = received_data;
    }

    return true;
}

/**
 * @brief 等待 SPI3 完成当前字节传输。
 * @param timeout_count 等待 BSY 清零的最大轮询次数。
 * @return SPI3 空闲返回 true，超时返回 false。
 */
bool SPI3_WaitIdle(uint32_t timeout_count)
{
    return SPI3_WaitForFlag(SPI_FLAG_BSY, RESET, timeout_count);
}
