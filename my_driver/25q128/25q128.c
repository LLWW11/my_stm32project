#include "25q128.h"
#include "SPI.h"

#define W25Q_CMD_WRITE_ENABLE  0x06U
#define W25Q_CMD_READ_SR1      0x05U
#define W25Q_CMD_READ_JEDEC_ID 0x9FU
#define W25Q_CMD_READ_DATA     0x03U
#define W25Q_CMD_PAGE_PROGRAM  0x02U
#define W25Q_CMD_SECTOR_ERASE  0x20U

#define W25Q_SR1_BUSY_MASK 0x01U
#define W25Q_SR1_WEL_MASK  0x02U

// W25Q128 的片选为 PB14，低电平有效 
#define W25Q_CS_LOW()  GPIO_ResetBits(GPIOB, GPIO_Pin_14)
#define W25Q_CS_HIGH() GPIO_SetBits(GPIOB, GPIO_Pin_14)

// 检查一段地址范围是否完全位于 W25Q128 容量以内
static bool W25Q_IsRangeValid(uint32_t address, // 起始地址
                              uint32_t length)  // 数据长度
{
    if (length == 0U)
    {
        return address <= W25Q128_CAPACITY_BYTES;
    }

    if (address >= W25Q128_CAPACITY_BYTES)
    {
        return false;
    }

    return length <= (W25Q128_CAPACITY_BYTES - address);
}


 //成功返回 W25Q_OK
static w25q_status_t W25Q_TransferByte(uint8_t transmit_data,
                                       uint8_t *receive_data)
{
    if (!SPI3_TransferByte(transmit_data, receive_data,
                           W25Q128_SPI_TIMEOUT_COUNT))
    {
        return W25Q_ERROR_SPI_TIMEOUT;
    }

    return W25Q_OK;
}

// 释放片段
static w25q_status_t W25Q_EndTransaction(void)
{
    bool spi_idle;

    spi_idle = SPI3_WaitIdle(W25Q128_SPI_TIMEOUT_COUNT);
    W25Q_CS_HIGH();

    return spi_idle ? W25Q_OK : W25Q_ERROR_SPI_TIMEOUT;
}

// 读取 W25Q128 状态寄存器 1
static w25q_status_t W25Q_ReadStatus1(uint8_t *status_register)
{
    w25q_status_t status;

    if (status_register == NULL)
    {
        return W25Q_ERROR_PARAMETER;
    }

    W25Q_CS_LOW();
    status = W25Q_TransferByte(W25Q_CMD_READ_SR1, NULL);
    if (status == W25Q_OK)
    {
        status = W25Q_TransferByte(0xFFU, status_register);
    }

    if ((W25Q_EndTransaction() != W25Q_OK) && (status == W25Q_OK))
    {
        status = W25Q_ERROR_SPI_TIMEOUT;
    }

    return status;
}

// 等待 W25Q128 完成内部擦除或编程操作
static w25q_status_t W25Q_WaitReady(uint32_t poll_limit)//最大轮询次数
{
    uint8_t status_register;
    w25q_status_t status;

    while (poll_limit > 0U)
    {
        status = W25Q_ReadStatus1(&status_register);
        if (status != W25Q_OK)
        {
            return status;
        }

        if ((status_register & W25Q_SR1_BUSY_MASK) == 0U)
        {
            return W25Q_OK;
        }

        poll_limit--;
    }

    return W25Q_ERROR_BUSY_TIMEOUT;
}

static w25q_status_t W25Q_WriteEnable(void) //写使能
{
    uint8_t status_register;
    w25q_status_t status;

    W25Q_CS_LOW();
    status = W25Q_TransferByte(W25Q_CMD_WRITE_ENABLE, NULL);

    if ((W25Q_EndTransaction() != W25Q_OK) && (status == W25Q_OK))
    {
        status = W25Q_ERROR_SPI_TIMEOUT;
    }

    if (status != W25Q_OK)
    {
        return status;
    }

    status = W25Q_ReadStatus1(&status_register);
    if (status != W25Q_OK)
    {
        return status;
    }

    return (status_register & W25Q_SR1_WEL_MASK) != 0U
               ? W25Q_OK
               : W25Q_ERROR_WRITE_ENABLE;
}

//写地址
static w25q_status_t W25Q_SendAddress(uint32_t address)
{
    w25q_status_t status;

    status = W25Q_TransferByte((uint8_t)(address >> 16), NULL);
    if (status != W25Q_OK)
    {
        return status;
    }

    status = W25Q_TransferByte((uint8_t)(address >> 8), NULL);
    if (status != W25Q_OK)
    {
        return status;
    }

    return W25Q_TransferByte((uint8_t)address, NULL);
}

static w25q_status_t W25Q_PageProgram(uint32_t address,//页内起始地址
                                      const uint8_t *data,//数据
                                      uint32_t length)//写入长度，不能跨页
{
    uint32_t index;
    w25q_status_t status;

    if ((data == NULL) || (length == 0U))
        return W25Q_ERROR_PARAMETER;

    if (!W25Q_IsRangeValid(address, length))
        return W25Q_ERROR_OUT_OF_RANGE;

    if ((length > W25Q128_PAGE_SIZE) ||
        ((address & (W25Q128_PAGE_SIZE - 1U)) + length > W25Q128_PAGE_SIZE))
        return W25Q_ERROR_PARAMETER;

    status = W25Q_WriteEnable();
    if (status != W25Q_OK)
        return status;

    W25Q_CS_LOW();
    status = W25Q_TransferByte(W25Q_CMD_PAGE_PROGRAM, NULL);
    if (status == W25Q_OK)
        status = W25Q_SendAddress(address);

    for (index = 0U; (index < length) && (status == W25Q_OK); index++)
        status = W25Q_TransferByte(data[index], NULL);

    if ((W25Q_EndTransaction() != W25Q_OK) && (status == W25Q_OK))
        status = W25Q_ERROR_SPI_TIMEOUT;

    if (status != W25Q_OK)
        return status;

    return W25Q_WaitReady(W25Q128_PAGE_PROGRAM_POLL_LIMIT);
}

//初始化 SPI3 与 W25Q128
w25q_status_t W25Q_Init(void)
{
    uint32_t id;
    w25q_status_t status;

    SPI3_Init();

    status = W25Q_WaitReady(W25Q128_SECTOR_ERASE_POLL_LIMIT);
    if (status != W25Q_OK)
        return status;

    status = W25Q_ReadId(&id);
    if (status != W25Q_OK)
        return status;

    return id == W25Q128_JEDEC_ID ? W25Q_OK : W25Q_ERROR_ID_MISMATCH;
}

//读取 W25Q128 的 JEDEC ID
w25q_status_t W25Q_ReadId(uint32_t *id)
{
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
    w25q_status_t status;

    if (id == NULL)
        return W25Q_ERROR_PARAMETER;

    W25Q_CS_LOW();
    status = W25Q_TransferByte(W25Q_CMD_READ_JEDEC_ID, NULL);
    if (status == W25Q_OK)
        status = W25Q_TransferByte(0xFFU, &manufacturer_id);
    if (status == W25Q_OK)
        status = W25Q_TransferByte(0xFFU, &memory_type);
    if (status == W25Q_OK)
        status = W25Q_TransferByte(0xFFU, &capacity_id);

    if ((W25Q_EndTransaction() != W25Q_OK) && (status == W25Q_OK))
        status = W25Q_ERROR_SPI_TIMEOUT;

    if (status == W25Q_OK)
        *id = ((uint32_t)manufacturer_id << 16) |
              ((uint32_t)memory_type << 8) |
              (uint32_t)capacity_id;

    return status;
}

w25q_status_t W25Q_Read(uint32_t address,   //W25Q128 内部起始地址
                        uint8_t *data,      //接收缓冲区
                        uint32_t length)    //读取长度
{
    uint32_t index;
    w25q_status_t status;

    if (length == 0U)
        return W25Q_OK;

    if (data == NULL)
        return W25Q_ERROR_PARAMETER;

    if (!W25Q_IsRangeValid(address, length))
        return W25Q_ERROR_OUT_OF_RANGE;

    status = W25Q_WaitReady(W25Q128_SECTOR_ERASE_POLL_LIMIT);
    if (status != W25Q_OK)
        return status;


    W25Q_CS_LOW();
    status = W25Q_TransferByte(W25Q_CMD_READ_DATA, NULL);
    if (status == W25Q_OK)
        status = W25Q_SendAddress(address);

    for (index = 0U; (index < length) && (status == W25Q_OK); index++)
        status = W25Q_TransferByte(0xFFU, &data[index]);

    if ((W25Q_EndTransaction() != W25Q_OK) && (status == W25Q_OK))
        status = W25Q_ERROR_SPI_TIMEOUT;

    return status;
}
// 擦除地址所在的 4 KiB 扇区
w25q_status_t W25Q_EraseSector(uint32_t address)
{
    uint32_t sector_address;
    w25q_status_t status;

    if (!W25Q_IsRangeValid(address, 1U))
        return W25Q_ERROR_OUT_OF_RANGE;

    sector_address = address & ~(W25Q128_SECTOR_SIZE - 1U);
    status = W25Q_WaitReady(W25Q128_SECTOR_ERASE_POLL_LIMIT);
    if (status != W25Q_OK)
        return status;

    status = W25Q_WriteEnable();
    if (status != W25Q_OK)
        return status;

    W25Q_CS_LOW();
    status = W25Q_TransferByte(W25Q_CMD_SECTOR_ERASE, NULL);
    if (status == W25Q_OK)
        status = W25Q_SendAddress(sector_address);

    if ((W25Q_EndTransaction() != W25Q_OK) && (status == W25Q_OK))
        status = W25Q_ERROR_SPI_TIMEOUT;

    if (status != W25Q_OK)
        return status;

    return W25Q_WaitReady(W25Q128_SECTOR_ERASE_POLL_LIMIT);
}

//调用前必须确保目标区域已擦除
w25q_status_t W25Q_Write(uint32_t address,//内部起始地址
                         const uint8_t *data,
                         uint32_t length)//写入长度
{
    uint32_t page_remaining;
    uint32_t chunk_length;
    w25q_status_t status;

    if (length == 0U)
        return W25Q_OK;

    if (data == NULL)
        return W25Q_ERROR_PARAMETER;

    if (!W25Q_IsRangeValid(address, length))
        return W25Q_ERROR_OUT_OF_RANGE;

    while (length > 0U)
    {
        page_remaining = W25Q128_PAGE_SIZE -
                         (address & (W25Q128_PAGE_SIZE - 1U));
        chunk_length = length < page_remaining ? length : page_remaining;

        status = W25Q_PageProgram(address, data, chunk_length);
        if (status != W25Q_OK)
        {
            return status;
        }

        address += chunk_length;
        data += chunk_length;
        length -= chunk_length;
    }

    return W25Q_OK;
}
