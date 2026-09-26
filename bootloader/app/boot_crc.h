#ifndef WEATHERCLOCK_BOOT_CRC_H
#define WEATHERCLOCK_BOOT_CRC_H

#include <stdint.h>

/** 按 CRC-32/ISO-HDLC 规则更新中间状态，初值使用 0xFFFFFFFF。 */
uint32_t boot_crc32_update(uint32_t crc, const uint8_t *data, uint32_t length);

/** 对一段数据计算与 Python binascii.crc32 一致的 CRC32。 */
uint32_t boot_crc32(const uint8_t *data, uint32_t length);

#endif /* WEATHERCLOCK_BOOT_CRC_H */
