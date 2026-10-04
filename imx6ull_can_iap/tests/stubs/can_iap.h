#ifndef TEST_CAN_IAP_H
#define TEST_CAN_IAP_H
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY UINT32_MAX
typedef struct { uint32_t StdId; uint8_t DLC; uint8_t Data[8]; } CanRxMsg;
/** 主机测试替代硬件初始化。 */
bool can_test_init(bool loopback);
/** 主机测试替代测试滤波器。 */
void can_test_enable_probe_filter(void);
/** 捕获实际 APP 编码出的 ACK。 */
bool can_test_send(uint16_t id, const uint8_t *data, uint8_t len, TickType_t timeout);
/** 主机测试不运行接收任务，固定返回无报文。 */
bool can_test_receive(CanRxMsg *rx, TickType_t timeout);
#endif
