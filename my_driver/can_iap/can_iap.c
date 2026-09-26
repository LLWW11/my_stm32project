#include "can_iap.h"

#include <string.h>
#include "task.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

// 配置 CAN1 的 PA11/PA12、500 kbit/s 位时序及 FIFO0 接收滤波器
bool can_test_init(bool loopback)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can;
    CAN_FilterInitTypeDef filter;
    RCC_ClocksTypeDef clocks;

    RCC_GetClocksFreq(&clocks);
    RCC_GetClocksFreq(&clocks);
    if (clocks.PCLK1_Frequency != 42000000U)
        return false; //  42 MHz 的 APB1 时钟

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_CAN1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_CAN1);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    CAN_StructInit(&can);
    can.CAN_Mode = loopback ? CAN_Mode_LoopBack : CAN_Mode_Normal;
    can.CAN_SJW = CAN_SJW_1tq; // 同步跳转宽度
    can.CAN_BS1 = CAN_BS1_11tq;
    can.CAN_BS2 = CAN_BS2_2tq;
    can.CAN_Prescaler = 6;
    can.CAN_ABOM = ENABLE;  // 总线关闭后自动恢复
    can.CAN_NART = DISABLE; // 正常模式下允许自动重发
    if (CAN_Init(CAN1, &can) != CAN_InitStatus_Success)
        return false;
    CAN_SlaveStartBank(14); // 滤波器组0~13

    // 测试阶段所有iD都接受
    memset(&filter, 0, sizeof(filter));
    filter.CAN_FilterNumber = 0;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&filter);
    return true;
}

bool can_test_send(uint16_t id, const uint8_t *data, uint8_t len, TickType_t timeout_ticks)
{
    CanTxMsg tx;
    uint8_t mailbox;
    uint8_t status;
    TickType_t start;
    if (id > 0x7ffU || len > 8 || (len > 8 && data == NULL))
        return false;
    memset(&tx, 0, sizeof(tx));
    tx.StdId = id;
    tx.DLC = len;
    tx.RTR = CAN_RTR_Data;
    tx.IDE = CAN_Id_Standard;
    if (len > 0U)
        memcpy(tx.Data, data, len);

    mailbox = CAN_Transmit(CAN1, &tx);
    if (mailbox == CAN_TxStatus_NoMailBox)
        return false;

    start = xTaskGetTickCount();
    while (1)
    {
        status = CAN_TransmitStatus(CAN1, mailbox);
        if (status == CAN_TxStatus_Ok)
            return true;
        if (status == CAN_TxStatus_Failed)
            return false;
        if ((TickType_t)(xTaskGetTickCount() - start) >= timeout_ticks)
        {
            CAN_CancelTransmit(CAN1, mailbox);
            return false;
        }
        vTaskDelay(1);
    }
}

bool can_test_receive(CanRxMsg *rx, TickType_t timeout_ticks)
{
    TickType_t start;

    if (rx == NULL)
        return false;

    start = xTaskGetTickCount();
    while (1)
    {
        while (CAN_MessagePending(CAN1, CAN_FIFO0) > 0U)
        {
            CAN_Receive(CAN1, CAN_FIFO0, rx);
            if (rx->IDE == CAN_Id_Standard &&
                rx->RTR == CAN_RTR_Data &&
                rx->DLC <= 8U)
                return true;
        }

        if ((TickType_t)(xTaskGetTickCount() - start) >= timeout_ticks)
            return false;
        vTaskDelay(1);
    }
}

