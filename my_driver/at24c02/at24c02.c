#include "at24c02.h"
#include "IIC.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* 检查一段地址范围是否完全落在 256 字节容量内（写法对齐 W25Q_IsRangeValid） */
static bool AT24C02_IsRangeValid(uint16_t address, // 起始地址
                                 uint32_t length)  // 数据长度
{
    if (length == 0U)
    {
        return address <= AT24C02_CAPACITY_BYTES;
    }

    if (address >= AT24C02_CAPACITY_BYTES)
    {
        return false;
    }

    return length <= (AT24C02_CAPACITY_BYTES - address);
}

at24c02_status_t AT24C02_WaitReady(void)
{
    uint32_t probe;

    /*
     * AT24C02 在内部擦写期间不会应答自己的器件地址，也不拉低 SDA。
     * 所以"能 ACK"就等于"写完了"—— 比固定 delay(5ms) 快得多，也不怕器件比别人慢。
     *
     * 每次探测本身就要发 START + 8 拍地址 + 1 拍应答 + STOP，按 100kHz 算约 110us，
     * 100 次上限约 10ms，足够覆盖 5ms 的最坏写周期。
     *
     * 关于"让不让出 CPU"：
     *   这是毫秒级的等待，可以也应该交给 RTOS。但也不能一上来就 vTaskDelay ——
     *   tick 粒度是 1ms，而写周期典型只有一两个毫秒，一上来就切出去反而会给
     *   每次页写平白多加 1ms（一次 24 字节记录 = 3 次页写 = 多 3ms）。
     *   所以分两段：先靠探测自带的时钟快速试几次（约 1ms 以内就能覆盖大多数情况），
     *   连续失败才 vTaskDelay 让出。这样既不浪费 tick，也不会长时间独占总线把
     *   低优先级任务（比如 mloop）饿死。
     *
     * 注意：让出 CPU 之后就允许别的任务运行了。如果将来有第二个使用者要访问
     * 同一条 I2C 总线，必须在更外层加互斥，否则这里会让出到一半被插进来。
     */
    for (probe = 0U; probe < AT24C02_READY_PROBE_MAX; ++probe)
    {
        if (IIC1_Probe(AT24C02_I2C_ADDR))
        {
            return AT24C02_OK;
        }

        if ((probe + 1U) >= AT24C02_READY_FAST_PROBES)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    return AT24C02_ERROR_NACK;
}

at24c02_status_t AT24C02_Init(void)
{
    IIC1_Init();

    /* 上电先探一下器件在不在。不在就直接返回，别让上层每次都白等 */
    if (!IIC1_Probe(AT24C02_I2C_ADDR))
    {
        return AT24C02_ERROR_NACK;
    }

    return AT24C02_OK;
}

at24c02_status_t AT24C02_ReadBuffer(uint16_t address,
                                    uint8_t *data,
                                    uint32_t length)
{
    uint8_t word_address;

    if ((data == NULL) && (length != 0U))
    {
        return AT24C02_ERROR_PARAMETER;
    }

    if (!AT24C02_IsRangeValid(address, length))
    {
        return AT24C02_ERROR_OUT_OF_RANGE;
    }

    if (length == 0U)
    {
        return AT24C02_OK;
    }

    /* 256 字节器件的字地址就是一个字节 */
    word_address = (uint8_t)(address & 0xFFU);

    /*
     * 随机读 = 先用"写"的方向把字地址送进去（这一步不写数据，只是设置器件内部的
     * 地址指针），再用重复起始转成读方向。中间绝对不能插 STOP。
     */
    if (!IIC1_WriteReadBytes(AT24C02_I2C_ADDR,
                               &word_address, 1U,
                               data, length))
    {
        return AT24C02_ERROR_NACK;
    }

    return AT24C02_OK;
}

at24c02_status_t AT24C02_WriteBuffer(uint16_t address,
                                     const uint8_t *data,
                                     uint32_t length)
{
    uint32_t written = 0U;

    if (data == NULL)
    {
        return AT24C02_ERROR_PARAMETER;
    }

    if (!AT24C02_IsRangeValid(address, length))
    {
        return AT24C02_ERROR_OUT_OF_RANGE;
    }

    /*
     * AT24C02 最经典的坑：器件内部地址计数器只有 3 位（8 字节一页）。
     *
     * 在一次**事务内部**连续写超过页边界的字节时，多出来的字节不会进位到下一页，
     * 而是"回卷"到本页开头，覆盖前面刚写的数据 —— 而且不会有任何错误返回。
     * 举例：一次性从 0x06 写 10 个字节，落在 0x06 0x07 然后回卷，最终 0x00~0x07
     * 里只剩最后 8 个字节，前面两个被冲掉了。
     *
     * 所以必须由驱动把跨页的写拆成若干次"不跨页"的事务：
     * 每段写到页边界就发 STOP（这一段落盘），下一段从下一页的基址重新开始。
     * 拆开之后，字节是按顺序连续落盘的 —— 从 0x06 写 4 个字节就是 0x06 0x07 0x08 0x09。
     *
     * 每页写完之后还要等写周期结束，才能开始下一段，否则第二段会被器件忽略。
     */
    while (written < length)
    {
        uint16_t current = (uint16_t)(address + written);
        uint32_t room_to_page_end = AT24C02_PAGE_SIZE - (current % AT24C02_PAGE_SIZE);
        uint32_t chunk = length - written;
        /* 一个字地址 + 最多 8 个数据字节，一次最多也就 9 个 */
        uint8_t payload[AT24C02_PAGE_SIZE + 1U];
        at24c02_status_t status;

        if (chunk > room_to_page_end)
        {
            chunk = room_to_page_end;
        }

        payload[0] = (uint8_t)(current & 0xFFU);
        memcpy(&payload[1], &data[written], chunk);

        if (!IIC1_WriteBytes(AT24C02_I2C_ADDR, payload, chunk + 1U))
        {
            return AT24C02_ERROR_NACK;
        }

        status = AT24C02_WaitReady();
        if (status != AT24C02_OK)
        {
            return status;
        }

        written += chunk;
    }

    return AT24C02_OK;
}

at24c02_status_t AT24C02_Fill(uint16_t address,
                              uint8_t value,
                              uint32_t length)
{
    uint8_t buffer[AT24C02_PAGE_SIZE];
    uint32_t written = 0U;

    if (!AT24C02_IsRangeValid(address, length))
    {
        return AT24C02_ERROR_OUT_OF_RANGE;
    }

    memset(buffer, value, sizeof(buffer));

    /* 按页整页写，比一字节一字节写省掉大量写周期等待 */
    while (written < length)
    {
        uint32_t chunk = length - written;
        at24c02_status_t status;

        if (chunk > AT24C02_PAGE_SIZE)
        {
            chunk = AT24C02_PAGE_SIZE;
        }

        status = AT24C02_WriteBuffer((uint16_t)(address + written), buffer, chunk);
        if (status != AT24C02_OK)
        {
            return status;
        }

        written += chunk;
    }

    return AT24C02_OK;
}

at24c02_status_t AT24C02_SelfTest(void)
{
    uint8_t original[AT24C02_PAGE_SIZE];
    uint8_t pattern[AT24C02_PAGE_SIZE];
    uint8_t readback[AT24C02_PAGE_SIZE];
    uint32_t index;
    at24c02_status_t status;

    status = AT24C02_ReadBuffer(AT24C02_SELFTEST_ADDR, original, AT24C02_PAGE_SIZE);
    if (status != AT24C02_OK)
    {
        return status;
    }

    /*
     * 测试数据取"原值的按位取反"，这样保证要写的值一定和现在的值不同。
     * 这一步是能检出 WP 写保护的关键：
     *   WP 拉高时，AT24C02 对写命令照样回 ACK，只是内部不真正擦写 ——
     *   也就是说 IIC1_WriteBytes 会返回成功、写周期也会正常结束，
     *   只有"读回来还是旧值"这个现象能暴露它。
     * 如果测试数据有可能等于原值，就存在"写保护了却恰好比对通过"的漏检。
     */
    for (index = 0U; index < AT24C02_PAGE_SIZE; ++index)
    {
        pattern[index] = (uint8_t)(~original[index]);
    }

    status = AT24C02_WriteBuffer(AT24C02_SELFTEST_ADDR, pattern, AT24C02_PAGE_SIZE);
    if (status != AT24C02_OK)
    {
        return status;
    }

    status = AT24C02_ReadBuffer(AT24C02_SELFTEST_ADDR, readback, AT24C02_PAGE_SIZE);
    if (status != AT24C02_OK)
    {
        return status;
    }

    if (memcmp(pattern, readback, AT24C02_PAGE_SIZE) != 0)
    {
        /* 尽量把原值抢救回去，虽然大概率也写不进 */
        (void)AT24C02_WriteBuffer(AT24C02_SELFTEST_ADDR, original, AT24C02_PAGE_SIZE);
        return AT24C02_ERROR_VERIFY;
    }

    /* 自检不留痕迹：把原值写回去 */
    return AT24C02_WriteBuffer(AT24C02_SELFTEST_ADDR, original, AT24C02_PAGE_SIZE);
}
