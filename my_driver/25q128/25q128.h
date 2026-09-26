#ifndef WEATHERCLOCK_W25Q128_H
#define WEATHERCLOCK_W25Q128_H

#include <stdint.h>

#define W25Q128_JEDEC_ID                0x00EF4018UL
#define W25Q128_CAPACITY_BYTES          0x01000000UL
#define W25Q128_SECTOR_SIZE             4096UL
#define W25Q128_PAGE_SIZE               256UL
#define W25Q128_SPI_TIMEOUT_COUNT       100000UL
#define W25Q128_PAGE_PROGRAM_POLL_LIMIT 50000UL
#define W25Q128_SECTOR_ERASE_POLL_LIMIT 1000000UL

/**
 * @brief W25Q128 驱动操作结果。
 */
typedef enum
{
    W25Q_OK = 0,
    W25Q_ERROR_PARAMETER,
    W25Q_ERROR_OUT_OF_RANGE,
    W25Q_ERROR_SPI_TIMEOUT,
    W25Q_ERROR_BUSY_TIMEOUT,
    W25Q_ERROR_WRITE_ENABLE,
    W25Q_ERROR_ID_MISMATCH
} w25q_status_t;


w25q_status_t W25Q_Init(void);
w25q_status_t W25Q_ReadId(uint32_t *id);
w25q_status_t W25Q_Read(uint32_t address,
                        uint8_t *data,
                        uint32_t length);
w25q_status_t W25Q_EraseSector(uint32_t address);
w25q_status_t W25Q_Write(uint32_t address,
                         const uint8_t *data,
                         uint32_t length);

#endif /* WEATHERCLOCK_W25Q128_H */
