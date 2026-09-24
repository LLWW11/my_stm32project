#include "../bootloader/app/boot_crc.h"
#include <stdint.h>
#include <stdio.h>

/** 验证 MCU 软件 CRC32 与标准校验值以及分块更新结果一致。 */
int main(void)
{
    static const uint8_t data[] = "123456789";
    uint32_t whole = boot_crc32(data, 9U);
    uint32_t pieces = boot_crc32_update(0xFFFFFFFFU, data, 4U);

    pieces = boot_crc32_update(pieces, data + 4U, 5U) ^ 0xFFFFFFFFU;
    if ((whole != 0xCBF43926U) || (pieces != whole))
    {
        fprintf(stderr, "CRC32 mismatch: whole=%08lX pieces=%08lX\n",
                (unsigned long)whole, (unsigned long)pieces);
        return 1;
    }

    puts("boot CRC32 PASS");
    return 0;
}
