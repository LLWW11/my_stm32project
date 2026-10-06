#include "stm32f4xx.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"
#include "IIC.h"
#define IIC_SDA GPIO_Pin_0
#define IIC_SCL GPIO_Pin_1


#define I2C1_SDA GPIO_Pin_9
#define I2C1_SCL GPIO_Pin_8

// #define IIC2_CHECK_EVENT(EVENT, TIMEOUT)                    \
//     do                                                      \
//     {                                                       \
//         timeout = TIMEOUT;                                  \
//         while (!I2C_CheckEvent(I2C2, EVENT) && timeout > 0) \
//         {                                                   \
//             vTaskDelay(pdMS_TO_TICKS(1));                   \
//             timeout -= 1000;                                \
//         }                                                   \
//         if (timeout <= 0)                                   \
//             return false;                                   \
//     } while (0);
/*
硬件连接：
SDA：PF0
SCL：PF1
*/
void IIC2_Init() // I2C2用于读写AHT20
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_Pin = IIC_SDA | IIC_SCL;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource1, GPIO_AF_I2C2);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource0, GPIO_AF_I2C2);

    I2C_InitTypeDef IIC2_InitStruct;
    I2C_StructInit(&IIC2_InitStruct);
    IIC2_InitStruct.I2C_Ack = I2C_Ack_Enable;
    IIC2_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    IIC2_InitStruct.I2C_ClockSpeed = 100UL * 1000UL;
    IIC2_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
    IIC2_InitStruct.I2C_Mode = I2C_Mode_I2C;
    IIC2_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_Init(I2C2, &IIC2_InitStruct);
    I2C_Cmd(I2C2, ENABLE);
}


void IIC1_Init() // I2C1用于读写AT24C02
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_Pin = I2C1_SCL | I2C1_SDA;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_I2C1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

    I2C_InitTypeDef IIC1_InitStruct;
    I2C_StructInit(&IIC1_InitStruct);
    IIC1_InitStruct.I2C_Ack = I2C_Ack_Enable;
    IIC1_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    IIC1_InitStruct.I2C_ClockSpeed = 100UL * 1000UL;
    IIC1_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
    IIC1_InitStruct.I2C_Mode = I2C_Mode_I2C;
    IIC1_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_Init(I2C1, &IIC1_InitStruct);
    I2C_Cmd(I2C1, ENABLE);

}
/**
static bool IIC2_SentData(uint8_t IIC_SendData, bool isAddr)
{
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    int timeout = 100;
    I2C_GenerateSTART(I2C2, ENABLE);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, timeout);
    if (isAddr == true)
    {
        I2C_Send7bitAddress(I2C2, IIC_SendData, I2C_Direction_Transmitter);
        IIC2_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, timeout);
    }
    else
    {
        I2C_SendData(I2C2, IIC_SendData);
        IIC2_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTED, timeout);
    }

    // I2C_GenerateSTOP(I2C2, ENABLE);
    return true;
}

static bool IIC2_ReceiveData(uint8_t Addr, uint8_t *dat, uint8_t len)
{
    int timeout = 100;
    I2C_GenerateSTART(I2C2, ENABLE);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, timeout);
    I2C_Send7bitAddress(I2C2, 0x38, I2C_Direction_Transmitter);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, timeout);
    for (uint8_t i = 0; i < len; i++)
    {
        if (i == len - 1)
            // 最后一个字节，禁用ACK
            I2C_AcknowledgeConfig(I2C2, DISABLE);
        IIC2_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED, timeout);
        *(dat + i) = I2C_ReceiveData(I2C2);
    }
    I2C_GenerateSTOP(I2C2, ENABLE);
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    return true;
}
*/


#define IIC1_TIMEOUT_LOOPS 200000UL

/* 等总线空闲BUSY 一直置位说明上一次事务没收干净 */
#define IIC1_WAIT_IDLE()                                     \
    do                                                       \
    {                                                        \
        uint32_t iic1_t = IIC1_TIMEOUT_LOOPS;                \
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))       \
        {                                                    \
            if (--iic1_t == 0U)                              \
                return false;                                \
        }                                                    \
    } while (0)

/* 等某个事件超时就补一个 STOP 把总线放开，再返回失败 */
#define IIC1_WAIT_EVENT(EVENT)                               \
    do                                                       \
    {                                                        \
        uint32_t iic1_t = IIC1_TIMEOUT_LOOPS;                \
        while (!I2C_CheckEvent(I2C1, (EVENT)))               \
        {                                                    \
            if (--iic1_t == 0U)                              \
            {                                                \
                I2C_GenerateSTOP(I2C1, ENABLE);              \
                return false;                                \
            }                                                \
        }                                                    \
    } while (0)

static void IIC1_RecoverDelay(void)
{
    volatile uint32_t spin;

    for (spin = 0U; spin < 400U; ++spin)
    {
        ;
    }
}

bool IIC1_BusRecover(void)
{
    GPIO_InitTypeDef gpio_init;
    uint8_t pulse;

    I2C_Cmd(I2C1, DISABLE);

    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_OType = GPIO_OType_OD;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    gpio_init.GPIO_Speed = GPIO_High_Speed;
    GPIO_Init(GPIOB, &gpio_init);

    GPIO_SetBits(GPIOB, GPIO_Pin_8 | GPIO_Pin_9);
    IIC1_RecoverDelay();

    for (pulse = 0U; pulse < 9U; ++pulse)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_8);
        IIC1_RecoverDelay();
        GPIO_SetBits(GPIOB, GPIO_Pin_8);
        IIC1_RecoverDelay();
    }

    /* 补 STOP：SCL 低 -> SDA 低 -> SCL 高 -> SDA 高
     * 必须先把 SCL 拉低再动 SDA，否则"SCL 高时拉低 SDA"会被当成 START */
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
    IIC1_RecoverDelay();
    GPIO_ResetBits(GPIOB, GPIO_Pin_9);
    IIC1_RecoverDelay();
    GPIO_SetBits(GPIOB, GPIO_Pin_8);
    IIC1_RecoverDelay();
    GPIO_SetBits(GPIOB, GPIO_Pin_9);
    IIC1_RecoverDelay();

    /* 切回复用功能并重新使能外设；配置寄存器在 DISABLE 期间是保留的，不用重新 I2C_Init */
    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_OD;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    gpio_init.GPIO_Speed = GPIO_High_Speed;
    GPIO_Init(GPIOB, &gpio_init);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_I2C1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

    I2C_Cmd(I2C1, ENABLE);

    return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) != 0U);
}

bool IIC1_Probe(uint8_t dev_addr_7bit)
{
    bool acked;

    IIC1_WAIT_IDLE();

    I2C_GenerateSTART(I2C1, ENABLE);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_MODE_SELECT);
    I2C_Send7bitAddress(I2C1, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Transmitter);

    {
        uint32_t timeout = IIC1_TIMEOUT_LOOPS;

        while ((I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == RESET) &&
               (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == RESET))
        {
            if (--timeout == 0U)
            {
                I2C_GenerateSTOP(I2C1, ENABLE);
                return false;
            }
        }

        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != RESET)
        {
            I2C_ClearFlag(I2C1, I2C_FLAG_AF);
            acked = false;
        }
        else
        {
            (void)I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY);
            acked = true;
        }
    }

    I2C_GenerateSTOP(I2C1, ENABLE);

    return acked;
}

bool IIC1_WriteBytes(uint8_t dev_addr_7bit,
                     const uint8_t *data,
                     uint32_t length)
{
    uint32_t index;

    if ((data == NULL) && (length != 0U))
    {
        return false;
    }

    IIC1_WAIT_IDLE();

    I2C_GenerateSTART(I2C1, ENABLE);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_MODE_SELECT);
    I2C_Send7bitAddress(I2C1, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Transmitter);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);

    for (index = 0U; index < length; ++index)
    {
        I2C_SendData(I2C1, data[index]);
        IIC1_WAIT_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTED);
    }

    I2C_GenerateSTOP(I2C1, ENABLE);

    return true;
}

bool IIC1_ReadBytes(uint8_t dev_addr_7bit,
                    uint8_t *data,
                    uint32_t length)
{
    uint32_t remain;

    if ((data == NULL) || (length == 0U))
    {
        return false;
    }

    IIC1_WAIT_IDLE();

    I2C_GenerateSTART(I2C1, ENABLE);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_MODE_SELECT);
    I2C_Send7bitAddress(I2C1, (uint8_t)((dev_addr_7bit << 1) | 0x01U), I2C_Direction_Receiver);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED); /* 这一步顺带清掉 ADDR */

    remain = length;
    while (remain > 0U)
    {
        if (remain == 1U)
        {
            I2C_AcknowledgeConfig(I2C1, DISABLE);
            I2C_GenerateSTOP(I2C1, ENABLE);
        }

        IIC1_WAIT_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED);
        *data = I2C_ReceiveData(I2C1);
        ++data;
        --remain;
    }

    I2C_AcknowledgeConfig(I2C1, ENABLE);

    return true;
}

bool IIC1_WriteReadBytes(uint8_t dev_addr_7bit,
                         const uint8_t *tx,
                         uint32_t tx_length,
                         uint8_t *rx,
                         uint32_t rx_length)
{
    uint32_t index;
    uint32_t remain;

    if ((rx == NULL) || (rx_length == 0U))
    {
        return false;
    }
    if ((tx == NULL) && (tx_length != 0U))
    {
        return false;
    }

    IIC1_WAIT_IDLE();

    I2C_GenerateSTART(I2C1, ENABLE);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_MODE_SELECT);
    I2C_Send7bitAddress(I2C1, (uint8_t)(dev_addr_7bit << 1), I2C_Direction_Transmitter);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);

    for (index = 0U; index < tx_length; ++index)
    {
        I2C_SendData(I2C1, tx[index]);
        IIC1_WAIT_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTED);
    }

    /* 重复起始：中间不发 STOP，器件内部地址指针才不会被复位 */
    I2C_GenerateSTART(I2C1, ENABLE);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_MODE_SELECT);
    I2C_Send7bitAddress(I2C1, (uint8_t)((dev_addr_7bit << 1) | 0x01U), I2C_Direction_Receiver);
    IIC1_WAIT_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED);

    remain = rx_length;
    while (remain > 0U)
    {
        if (remain == 1U)
        {
            I2C_AcknowledgeConfig(I2C1, DISABLE);
            I2C_GenerateSTOP(I2C1, ENABLE);
        }

        IIC1_WAIT_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED);
        *rx = I2C_ReceiveData(I2C1);
        ++rx;
        --remain;
    }

    I2C_AcknowledgeConfig(I2C1, ENABLE);

    return true;
}