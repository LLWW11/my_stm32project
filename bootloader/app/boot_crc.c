#include "boot_crc.h"

/** 使用反射多项式 0xEDB88320 更新 CRC32 中间值。 */
uint32_t boot_crc32_update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    uint32_t bit;

    if ((data == 0) && (length != 0U))
        return crc;

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
                crc = (crc >> 1) ^ 0xEDB88320U;
            else
                crc >>= 1;
        }
    }

    return crc;
}

/** 计算完整的 CRC-32/ISO-HDLC，供镜像头与数据块校验使用。 */
uint32_t boot_crc32(const uint8_t *data, uint32_t length)
{
    return boot_crc32_update(0xFFFFFFFFU, data, length) ^ 0xFFFFFFFFU;
}
