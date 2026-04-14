#include "IIC.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
// #define IIC2_CHECK_EVENT(EVENT, TIMEOUT)                    \
//     do                                                      \
//     {                                                       \
//         uint32_t timeout = TIMEOUT;                         \
//         while (!I2C_CheckEvent(I2C2, EVENT) && timeout > 0) \
//         {                                                   \
//             vTaskDelay(pdMS_TO_TICKS(1));                   \
//             timeout -= 1000;                                \
//         }                                                   \
//         if (timeout <= 0)                                   \
//             return false;                                   \
//     } while (0);
#define IIC2_CHECK_EVENT(EVENT, TIMEOUT)     \
    do                                       \
    {                                        \
        uint32_t timeout = TIMEOUT;          \
        while (!I2C_CheckEvent(I2C2, EVENT)) \
        {                                    \
            if (--timeout == 0)              \
                return false;                \
        }                                    \
                                             \
    } while (0);
#define TIMEOUT 0x3FFFF
static bool AHT20_write(uint8_t data[], uint32_t length);
static bool AHT20_read(uint8_t data[], uint32_t length);
static bool aht20_is_ready(void);

bool AHT20_Init(void)
{
    IIC2_Init();
    vTaskDelay(pdMS_TO_TICKS(40));
    if (aht20_is_ready())
        return true;

    if (!AHT20_write((uint8_t[]){0xBE, 0x08, 0x00}, 3))
        return false;

    for (uint32_t t = 0; t < 100; t++)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (aht20_is_ready())
            return true;
    }

    return false;
}

static bool AHT20_write(uint8_t data[], uint32_t len)
{
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTART(I2C2, ENABLE);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, TIMEOUT);
    I2C_Send7bitAddress(I2C2, 0x70, I2C_Direction_Transmitter);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, TIMEOUT);
    for (uint32_t i = 0; i < len; i++)
    {
        I2C_SendData(I2C2, data[i]);
        IIC2_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, TIMEOUT);
    }
    I2C_GenerateSTOP(I2C2, ENABLE);

    return true;
}
static bool AHT20_read(uint8_t data[], uint32_t len)
{
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTART(I2C2, ENABLE);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, TIMEOUT);
    I2C_Send7bitAddress(I2C2, 0x70, I2C_Direction_Receiver);
    IIC2_CHECK_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, TIMEOUT);
    for (uint32_t i = 0; i < len; i++)
    {
        if (i == len - 1)
            I2C_AcknowledgeConfig(I2C2, DISABLE);
        IIC2_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED, TIMEOUT);
        data[i] = I2C_ReceiveData(I2C2);
    }
    I2C_GenerateSTOP(I2C2, ENABLE);

    return true;
}
static bool aht20_read_status(uint8_t *status)
{
    // uint8_t cmd = 0x71;
    // if (!AHT20_write(&cmd, 1))
    //     return false;
    // if (!AHT20_write(status, 1))
    //     return false;

    // return true;
    return AHT20_read(status, 1);
}
static bool aht20_is_busy(void)
{
    uint8_t status;
    if (!aht20_read_status(&status))
        return false;
    return (status & 0x80) != 0;
}
static bool aht20_is_ready(void)
{
    uint8_t status;
    if (!aht20_read_status(&status))
        return false;
    return (status & 0x08) != 0;
}
bool aht20_start_measurement(void)
{
    return AHT20_write((uint8_t[]){0xAC, 0x33, 0x00}, 3);
}
bool aht20_wait_for_measurement(void)
{
    for (uint32_t t = 0; t < 200; t++)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (!aht20_is_busy())
            return true;
    }
    return false;
}
bool aht20_read_measurement(float *temperature, float *humidity)
{
    uint8_t data[6];
    if (!AHT20_read(data, 6))
        return false;

    uint32_t raw_humidity = ((uint32_t)data[1] << 12) |
                            ((uint32_t)data[2] << 4) |
                            ((uint32_t)(data[3] & 0xF0) >> 4);
    uint32_t raw_temperature = ((uint32_t)(data[3] & 0x0F) << 16) |
                               ((uint32_t)data[4] << 8) |
                               ((uint32_t)data[5]);

    *humidity = (float)raw_humidity * 100.0f / (float)0x100000;
    *temperature = (float)raw_temperature * 200.0f / (float)0x100000 - 50.0f;

    return true;
}
