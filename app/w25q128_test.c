#include "dbg_config.h"
#include "w25q128_test.h"
#include "25q128/25q128.h"

//在 RTOS 启动任务中执行 W25Q128 检查
void w25q128_self_test(void)
{
#if ((ENABLE_W25Q128_ID_TEST == 1) || (ENABLE_W25Q128_WRITE_TEST == 1))
    uint32_t jedec_id;

    jedec_id = W25Q_ReadID();
    printf("[W25Q128] JEDEC ID: 0x%06lX\r\n", (unsigned long)jedec_id);
    if (jedec_id != W25Q128_JEDEC_ID)
    {
        printf("[W25Q128] FAIL: unexpected JEDEC ID, erase/write skipped\r\n");
        return;
    }
    printf("[W25Q128] ID verify PASS\r\n");

#if (ENABLE_W25Q128_WRITE_TEST == 1)
    {
        static const uint8_t write_data[] = {
            0x57, 0x32, 0x35, 0x51, 0x31, 0x32, 0x38, 0x2D,
            0x52, 0x54, 0x4F, 0x53, 0x2D, 0x54, 0x45, 0x53,
            0x54, 0x00, 0xA5, 0x5A, 0x11, 0x22, 0x33, 0x44,
            0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xCC, 0xEE};
        uint8_t read_data[sizeof(write_data)];
        uint32_t index;

        printf("[W25Q128] Destructive test sector: 0x%06lX\r\n",(unsigned long)W25Q128_TEST_ADDRESS);
        W25Q_SectorErase(W25Q128_TEST_ADDRESS);
        W25Q_Read(W25Q128_TEST_ADDRESS, read_data, sizeof(read_data));
        for (index = 0; index < sizeof(read_data); index++)
        {
            if (read_data[index] != 0xFF)
            {
                printf("[W25Q128] FAIL: erase verify at byte %lu, value=0x%02X\r\n",
                       (unsigned long)index, read_data[index]);
                return;
            }
        }
        printf("[W25Q128] Erase verify PASS\r\n");

        W25Q_PageWrite(W25Q128_TEST_ADDRESS, write_data, sizeof(write_data));
        W25Q_Read(W25Q128_TEST_ADDRESS, read_data, sizeof(read_data));
        for (index = 0; index < sizeof(read_data); index++)
        {
            if (read_data[index] != write_data[index])
            {
                printf("[W25Q128] FAIL: compare at byte %lu, write=0x%02X read=0x%02X\r\n",
                       (unsigned long)index, write_data[index], read_data[index]);
                return;
            }
        }
        printf("[W25Q128] Write/read verify PASS\r\n");
    }
#endif
#endif
}
