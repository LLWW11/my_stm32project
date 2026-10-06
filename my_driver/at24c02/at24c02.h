#ifndef WEATHERCLOCK_AT24C02_H
#define WEATHERCLOCK_AT24C02_H

#include <stdint.h>

/*
 * AT24C02 驱动 —— 2 Kbit（256 字节）I2C EEPROM，挂在 PB8/PB9 的 IIC_SCL / IIC_SDA 上
 *
 * 硬件要点（EXPLORER_V3.5 原理图上的 U5）：
 *   - A0 / A1 / A2 三根地址脚在板上就近接地，所以 7 位器件地址 = 1010 000 = 0x50
 *   - WP（pin7）用于硬件写保护：拉高时"能收数据但不真正擦写"，而且不报任何错
 *     —— 这正是 AT24C02_ERROR_VERIFY 这个错误码存在的原因，
 *        排查方法见 AT24C02_SelfTest() 的说明。
 *   - 页大小只有 8 字节，且跨页写会回卷（详见 AT24C02_WriteBuffer 的注释）
 *
 * 与 W25Q128 的对比（这一点决定了本驱动的写法）：
 *   - EEPROM 支持字节随机改写，不需要擦除，所以"更新进度"是原地覆盖，不用追加日志
 *   - 写入后有最长 5ms 的内部写周期，期间器件不响应任何命令（包括自己的地址）
 */

#define AT24C02_CAPACITY_BYTES 256U
#define AT24C02_PAGE_SIZE      8U

/* 7 位从设备地址：高 4 位固定 1010，低 3 位由 A0~A2 决定（本板全部接地） */
#define AT24C02_BASE_ADDR 0x50U
#define AT24C02_ADDR_PINS 0x00U
#define AT24C02_I2C_ADDR  (AT24C02_BASE_ADDR | AT24C02_ADDR_PINS)

/* 数据手册标称写周期最大 5ms，轮询上限留到约 10ms 的余量 */
#define AT24C02_READY_PROBE_MAX 100U

/* 写周期等待的前若干次探测不主动让出 CPU（见 .c 里的说明） */
#define AT24C02_READY_FAST_PROBES 8U

/* 末页 8 字节保留给自检，业务数据不要放在这里 */
#define AT24C02_SELFTEST_ADDR 0xF8U

/**
 * @brief AT24C02 操作结果
 */
typedef enum
{
    AT24C02_OK = 0,
    AT24C02_ERROR_PARAMETER,
    AT24C02_ERROR_OUT_OF_RANGE,
    AT24C02_ERROR_NACK,   /* 从设备没应答：器件缺失 / 写周期未结束 / 总线被拉住 */
    AT24C02_ERROR_VERIFY  /* 写命令被 ACK 了，但读回来不是刚写的值：典型是 WP 被拉高 */
} at24c02_status_t;

/** 初始化底层 I2C 并探测器件是否存在。 */
at24c02_status_t AT24C02_Init(void);

/** 从 address 起读 length 字节，器件内部地址自动递增。 */
at24c02_status_t AT24C02_ReadBuffer(uint16_t address,
                                    uint8_t *data,
                                    uint32_t length);

/** 从 address 起写 length 字节，自动按 8 字节页边界拆分。 */
at24c02_status_t AT24C02_WriteBuffer(uint16_t address,
                                     const uint8_t *data,
                                     uint32_t length);

/** 用同一个值填充一段区域，常用于把记录区清成 0x00。 */
at24c02_status_t AT24C02_Fill(uint16_t address,
                              uint8_t value,
                              uint32_t length);

/** 轮询等待器件内部写周期结束（能应答 = 写完了）。 */
at24c02_status_t AT24C02_WaitReady(void);

/**
 * @brief 自检：验证器件在不在、能不能写、页写拆分对不对、能不能读
 *
 * 会临时改写末页 0xF8~0xFF 并在结束时恢复原值。
 * 返回 AT24C02_ERROR_VERIFY 基本就等价于"WP 被拉高了"。
 */
at24c02_status_t AT24C02_SelfTest(void);

#endif /* WEATHERCLOCK_AT24C02_H */
