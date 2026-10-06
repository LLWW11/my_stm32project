#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "can_iap.h"
/** 测试占位函数，实际协议测试直接调用 APP 的处理函数。 */
void vTaskDelete(void *task);
/** 测试占位函数，不执行真实休眠。 */
void vTaskDelay(TickType_t ticks);
/** 测试禁止执行硬件复位。 */
void NVIC_SystemReset(void);
#endif
