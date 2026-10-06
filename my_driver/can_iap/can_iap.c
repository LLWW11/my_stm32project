#include "can_iap.h"

#include <string.h>
#include "task.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

#include "queue.h"
#include "misc.h"

#define CAN_RX_QUEUE_LEN 16

static QueueHandle_t can_rx_queue = NULL;       // 队列作为缓冲区
static volatile uint32_t can_rx_drop_count = 0; // 丢包数量

// 配置 CAN1 的 PA11/PA12、500 kbit/s 位时序及 FIFO0 接收滤波器
bool can_test_init(bool loopback)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can;
    CAN_FilterInitTypeDef filter;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    if (can_rx_queue != NULL)
        return false;
    can_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(CanRxMsg));
    if (can_rx_queue == NULL)
        return false;

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_CAN1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_CAN1);

    CAN_StructInit(&can);
    can.CAN_Mode = loopback ? CAN_Mode_LoopBack : CAN_Mode_Normal;
    can.CAN_SJW = CAN_SJW_1tq; // 同步跳转宽度
    can.CAN_BS1 = CAN_BS1_11tq;
    can.CAN_BS2 = CAN_BS2_2tq;
    can.CAN_Prescaler = 6;
    can.CAN_ABOM = ENABLE;  // 总线关闭后自动恢复
    can.CAN_NART = DISABLE; // 正常模式下允许自动重发
    if (CAN_Init(CAN1, &can) != CAN_InitStatus_Success)
    {
        vQueueDelete(can_rx_queue);
        can_rx_queue = NULL;
        return false;
    }
    CAN_SlaveStartBank(14); // 滤波器组0~13

    // 测试模式接收测试 ID；正式模式只接收 START、DATA、END。
    memset(&filter, 0, sizeof(filter));
    filter.CAN_FilterNumber = 0; // 过滤器组0
    filter.CAN_FilterMode = CAN_FilterMode_IdList;
    filter.CAN_FilterScale = CAN_FilterScale_16bit;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0; // 匹配的报文存入 FIFO0
    filter.CAN_FilterActivation = ENABLE;

    filter.CAN_FilterIdHigh = (uint16_t)((loopback ? 0x320U : 0x120U) << 5);
    filter.CAN_FilterIdLow = (uint16_t)((loopback ? 0x321U : 0x121U) << 5);
    filter.CAN_FilterMaskIdHigh = (uint16_t)((loopback ? 0x322U : 0x122U) << 5);
    filter.CAN_FilterMaskIdLow = filter.CAN_FilterMaskIdHigh;
    CAN_FilterInit(&filter);

    NVIC_InitTypeDef nvic_can;
    nvic_can.NVIC_IRQChannel = CAN1_RX0_IRQn;
    nvic_can.NVIC_IRQChannelPreemptionPriority = 7;
    nvic_can.NVIC_IRQChannelSubPriority = 0;
    nvic_can.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_can);

    CAN_ClearITPendingBit(CAN1, CAN_IT_FOV0); // FIFO0过载中断标志位
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);  // 有帧等待读取
    CAN_ITConfig(CAN1, CAN_IT_FOV0, ENABLE);  // 过载

    return true;
}

/** 独立 CAN 测试模式使用的滤波器，禁止在正式 IAP 传输期间切换。 */
void can_test_enable_probe_filter(void)
{
    CAN_FilterInitTypeDef filter;
    memset(&filter, 0, sizeof(filter));
    filter.CAN_FilterNumber = 0;
    filter.CAN_FilterMode = CAN_FilterMode_IdList;
    filter.CAN_FilterScale = CAN_FilterScale_16bit;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    filter.CAN_FilterIdHigh = (uint16_t)(0x320U << 5);
    filter.CAN_FilterIdLow = (uint16_t)(0x321U << 5);
    filter.CAN_FilterMaskIdHigh = (uint16_t)(0x322U << 5);
    filter.CAN_FilterMaskIdLow = filter.CAN_FilterMaskIdHigh;
    CAN_FilterInit(&filter);
}

/** 搬运 FIFO0 标准数据帧到队列，记录溢出并唤醒等待任务。 */
void CAN1_RX0_IRQHandler(void)
{
    BaseType_t task_Woken = pdFALSE;
    if (CAN_GetITStatus(CAN1, CAN_IT_FOV0) != RESET)
    {
        can_rx_drop_count++;
        CAN_ClearITPendingBit(CAN1, CAN_IT_FOV0);
    }
    while (CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
    {
        CanRxMsg canRxFrame = {0};
        // memset(&canRxFrame, 0, sizeof(canRxFrame));
        CAN_Receive(CAN1, CAN_FIFO0, &canRxFrame);
        if (canRxFrame.IDE == CAN_Id_Standard && // 标准帧
            canRxFrame.RTR == CAN_RTR_DATA &&    // 数据帧
            canRxFrame.DLC <= 8)                 // 长度有效
        {
            if ((can_rx_queue == NULL) ||
                (xQueueSendFromISR(can_rx_queue, &canRxFrame, &task_Woken) != pdTRUE))
                can_rx_drop_count++;
        }
    }
    portYIELD_FROM_ISR(task_Woken);
}

/** 在任务上下文发送标准数据帧，超时取消未完成的硬件发送。 */
bool can_test_send(uint16_t id,
                   const uint8_t *data,
                   uint8_t len,
                   TickType_t timeout_ticks)
{
    CanTxMsg tx;
    uint8_t mailbox;
    uint8_t status;
    TickType_t start;
    if (id > 0x7ffU || len > 8 || (len > 0 && data == NULL))
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

/** 阻塞等待接收队列，由 RX0 中断投递报文后唤醒。 */
bool can_test_receive(CanRxMsg *rx, TickType_t timeout_ticks)
{
    if (rx == NULL || can_rx_queue == NULL)
        return false;
    return xQueueReceive(can_rx_queue, rx, timeout_ticks) == pdTRUE;
}
