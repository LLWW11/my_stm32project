#include "stm32f4xx.h"
#include "stdbool.h"
#include "delay.h"
#include "IIC.h"
#define IIC_SDA GPIO_Pin_0
#define IIC_SCL GPIO_Pin_1

#define IIC2_CHECK_EVENT(EVENT, TIMEOUT)                    \
	do                                                      \
	{                                                       \
		timeout = TIMEOUT;                                  \
		while (!I2C_CheckEvent(I2C2, EVENT) && timeout > 0) \
		{                                                   \
			SysTick_delay_us(1);                            \
			timeout -= 10;                                  \
		}                                                   \
		if (timeout <= 0)                                   \
			return false;                                   \
	} while (0);
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

/*
isAddr为1则是发地址，为0则是发数据
一次发一个字节数据,如果发送失败超时则返回false
*/
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
/*
Addr: 数据地址
dat:
*/
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
