#ifndef __DBG_CONFIG_H__
#define __DBG_CONFIG_H__

#include <stdio.h>

/* ========================================
 * 全局调试输出开关
 * 1: 开启所有串口调试打印
 * 0: 关闭所有串口调试打印
 * ======================================== */
#ifndef ENABLE_DEBUG_PRINT
#define ENABLE_DEBUG_PRINT 1
#endif
/* ========================================
 * 使用LVGL开关
 * 1: 开启
 * 0: 关闭
 * ======================================== */
#ifndef ENABLE_LVGL_USE
#define ENABLE_LVGL_USE 1
#endif
/* ========================================
 * W25Q128 上电只读测试开关
 * 1: 在 main_init 任务中读取并校验 JEDEC ID
 * 0: 跳过只读测试
 * ======================================== */
#ifndef ENABLE_W25Q128_ID_TEST
#define ENABLE_W25Q128_ID_TEST 0
#endif

/* ========================================
 * W25Q128 擦写测试开关
 * 1: 擦除末尾扇区并执行页写入、回读校验
 * 0: 仅执行上面的只读 ID 测试
 * 注意：启用前必须确认末尾 4 KiB 没有业务数据
 * ======================================== */
#ifndef ENABLE_W25Q128_WRITE_TEST
#define ENABLE_W25Q128_WRITE_TEST 0
#endif

/* ========================================
 * CAN 双向收发测试
 * 1: 使能测试
 * 0: 关闭测试,直接开启can相关任务
 * ======================================== */
#ifndef ENABLE_CAN_TEST
#define ENABLE_CAN_TEST 0
#endif

#endif
