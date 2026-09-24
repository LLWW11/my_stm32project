#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"
#include "boot_UART.h"
#include "boot_crc.h"
#include "boot_update.h"
#include "25q128/25q128.h"
#include <string.h>

#define BOOT_APP_BASE          0x08010000U
#define BOOT_APP_END           0x08100000U
#define BOOT_MAX_IMAGE_SIZE    0x000F0000U
#define BOOT_W25_HEADER_ADDR   0x00000000U
#define BOOT_W25_IMAGE_ADDR    0x00001000U
#define BOOT_IMAGE_MAGIC      0x31505557U /* 小端字节序为 WUP1 */
#define BOOT_STATE_READY       0xFFFFFFFEU
#define BOOT_STATE_DONE        0xFFFFFFFCU
#define BOOT_PACKET_SIZE       256U
#define BOOT_BYTE_TIMEOUT_MS   2000U

/** W25Q128 中的固定 24 字节镜像头，state 不参与头部 CRC。 */
typedef struct
{
    uint32_t magic;
    uint32_t target;
    uint32_t length;
    uint32_t image_crc;
    uint32_t header_crc;
    uint32_t state;
} boot_image_header_t;

/** 从小端字节数组读取一个 32 位整数。 */
static uint32_t boot_read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

/** 在每个字节具有独立超时的情况下读取指定长度。 */
static uint8_t boot_read_exact(uint8_t *data, uint32_t length)
{
    uint32_t index;

    for (index = 0U; index < length; index++)
    {
        if (boot_uart1_read_timeout(&data[index], BOOT_BYTE_TIMEOUT_MS) == 0U)
            return 0U;
    }

    return 1U;
}

/** 检查 BIN 开头的 MSP 与 Thumb 复位入口是否位于完整镜像中。 */
static uint8_t boot_image_vector_valid(const uint8_t *vector, uint32_t length)
{
    uint32_t msp = boot_read_u32(vector);
    uint32_t reset = boot_read_u32(vector + 4U);
    uint32_t reset_address = reset & ~1U;

    if ((msp < 0x20000000U) || (msp > 0x20020000U) || ((msp & 7U) != 0U))
        return 0U;
    if (((reset & 1U) == 0U) ||
        (reset_address < BOOT_APP_BASE + 8U) ||
        (reset_address >= BOOT_APP_BASE + length) ||
        (reset_address >= BOOT_APP_END))
        return 0U;
    return 1U;
}

/** 从 W25Q128 的镜像区分块计算完整 BIN CRC32。 */
static uint8_t boot_w25_image_crc(uint32_t length, uint32_t *crc_out)
{
    uint8_t buffer[BOOT_PACKET_SIZE];
    uint32_t offset;
    uint32_t count;
    uint32_t crc = 0xFFFFFFFFU;

    for (offset = 0U; offset < length; offset += count)
    {
        count = length - offset;
        if (count > BOOT_PACKET_SIZE)
            count = BOOT_PACKET_SIZE;
        if (W25Q_Read(BOOT_W25_IMAGE_ADDR + offset, buffer, count) != W25Q_OK)
            return 0U;
        crc = boot_crc32_update(crc, buffer, count);
    }

    *crc_out = crc ^ 0xFFFFFFFFU;
    return 1U;
}

/** 对元数据执行地址、长度、CRC 和状态检查。 */
static uint8_t boot_header_valid(const boot_image_header_t *header)
{
    if ((header->magic != BOOT_IMAGE_MAGIC) ||
        (header->target != BOOT_APP_BASE) ||
        (header->length < 8U) ||
        (header->length > BOOT_MAX_IMAGE_SIZE) ||
        (header->state != BOOT_STATE_READY))
        return 0U;

    return header->header_crc ==
           boot_crc32((const uint8_t *)header, 16U);
}

/** 擦除内部 Flash 的 APP 扇区 4 至 11，保留扇区 0 至 3。 */
static uint8_t boot_erase_app(void)
{
    static const uint16_t sectors[] = {
        FLASH_Sector_4, FLASH_Sector_5, FLASH_Sector_6, FLASH_Sector_7,
        FLASH_Sector_8, FLASH_Sector_9, FLASH_Sector_10, FLASH_Sector_11
    };
    uint32_t index;

    for (index = 0U; index < sizeof(sectors) / sizeof(sectors[0]); index++)
    {
        /* 本板使用 3.3 V，选用 2.7 V 至 3.6 V 的 32 位编程电压档。 */
        if (FLASH_EraseSector(sectors[index], VoltageRange_3) != FLASH_COMPLETE)
            return 0U;
        boot_uart1_send_string("[BOOT] Erased APP sector ");
        boot_uart1_send_hex32(index + 4U);
        boot_uart1_send_string("\r\n");
    }

    return 1U;
}

/** 将 W25 镜像写入内部 APP，最后才写入 MSP 和 Reset_Handler。 */
static uint8_t boot_program_app(const boot_image_header_t *header)
{
    uint8_t buffer[BOOT_PACKET_SIZE];
    uint8_t vector[8];
    uint32_t offset;
    uint32_t count;
    uint32_t index;
    uint32_t word;
    uint32_t address;
    uint32_t crc;

    if (W25Q_Read(BOOT_W25_IMAGE_ADDR, vector, sizeof(vector)) != W25Q_OK)
        return 0U;
    if (boot_image_vector_valid(vector, header->length) == 0U)
        return 0U;

    FLASH_Unlock();
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
        return 0U;

    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                    FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (boot_erase_app() == 0U)
    {
        FLASH_Lock();
        return 0U;
    }

    for (offset = 8U; offset < header->length; offset += count)
    {
        count = header->length - offset;
        if (count > BOOT_PACKET_SIZE)
            count = BOOT_PACKET_SIZE;
        if (W25Q_Read(BOOT_W25_IMAGE_ADDR + offset, buffer, count) != W25Q_OK)
        {
            FLASH_Lock();
            return 0U;
        }

        for (index = 0U; index < count; index += 4U)
        {
            /* 末尾不足 4 字节时，以已擦除的 0xFF 填充，不改变 BIN 的真实长度。 */
            word = 0xFFFFFFFFU;
            if (index + 0U < count) word = (word & 0xFFFFFF00U) | buffer[index];
            if (index + 1U < count) word = (word & 0xFFFF00FFU) | ((uint32_t)buffer[index + 1U] << 8);
            if (index + 2U < count) word = (word & 0xFF00FFFFU) | ((uint32_t)buffer[index + 2U] << 16);
            if (index + 3U < count) word = (word & 0x00FFFFFFU) | ((uint32_t)buffer[index + 3U] << 24);

            address = BOOT_APP_BASE + offset + index;
            if ((FLASH_ProgramWord(address, word) != FLASH_COMPLETE) ||
                (*(volatile uint32_t *)address != word))
            {
                FLASH_Lock();
                return 0U;
            }
        }
    }

    /* 前两个向量写入前，安装不完整的 APP 始终无法通过启动检查。 */
    for (index = 0U; index < 8U; index += 4U)
    {
        word = boot_read_u32(vector + index);
        address = BOOT_APP_BASE + index;
        if ((FLASH_ProgramWord(address, word) != FLASH_COMPLETE) ||
            (*(volatile uint32_t *)address != word))
        {
            FLASH_Lock();
            return 0U;
        }
    }

    FLASH_Lock();
    crc = boot_crc32((const uint8_t *)BOOT_APP_BASE, header->length);
    return crc == header->image_crc;
}

/** 检查外部 READY 镜像并在验证成功后安装到内部 Flash。 */
boot_update_result_t boot_update_install_pending(void)
{
    boot_image_header_t header;
    uint8_t vector[8];
    uint32_t crc;
    uint32_t done_state = BOOT_STATE_DONE;
    uint32_t readback_state;

    if (W25Q_Init() != W25Q_OK)
    {
        boot_uart1_send_string("[BOOT] W25 unavailable\r\n");
        return BOOT_UPDATE_NONE;
    }
    if (W25Q_Read(BOOT_W25_HEADER_ADDR, (uint8_t *)&header, sizeof(header)) != W25Q_OK)
        return BOOT_UPDATE_NONE;
    if (boot_header_valid(&header) == 0U)
        return BOOT_UPDATE_NONE;

    boot_uart1_send_string("[BOOT] W25 image READY\r\n");
    if ((boot_w25_image_crc(header.length, &crc) == 0U) ||
        (crc != header.image_crc) ||
        (W25Q_Read(BOOT_W25_IMAGE_ADDR, vector, sizeof(vector)) != W25Q_OK) ||
        (boot_image_vector_valid(vector, header.length) == 0U))
    {
        boot_uart1_send_string("[BOOT] External image invalid, APP kept\r\n");
        return BOOT_UPDATE_NONE;
    }

    boot_uart1_send_string("[BOOT] External CRC PASS\r\n");
    boot_uart1_send_string("[BOOT] Installing APP\r\n");
    if (boot_program_app(&header) == 0U)
    {
        boot_uart1_send_string("[BOOT] Install FAIL, stay in bootloader\r\n");
        return BOOT_UPDATE_FAILED;
    }

    boot_uart1_send_string("[BOOT] Internal CRC PASS\r\n");
    if ((W25Q_Write(BOOT_W25_HEADER_ADDR + 20U,
                    (const uint8_t *)&done_state, sizeof(done_state)) != W25Q_OK) ||
        (W25Q_Read(BOOT_W25_HEADER_ADDR + 20U,
                   (uint8_t *)&readback_state, sizeof(readback_state)) != W25Q_OK) ||
        (readback_state != BOOT_STATE_DONE))
    {
        boot_uart1_send_string("[BOOT] State update FAIL, retry on reset\r\n");
        return BOOT_UPDATE_FAILED;
    }

    boot_uart1_send_string("[BOOT] Install PASS, resetting\r\n");
    return BOOT_UPDATE_INSTALLED;
}

/** 接收 12 字节开始帧和分包数据，验证后写入 READY 元数据。 */
void boot_update_receive(void)
{
    uint8_t begin[12];
    uint8_t packet[BOOT_PACKET_SIZE];
    uint8_t verify[BOOT_PACKET_SIZE];
    uint8_t packet_crc_bytes[4];
    boot_image_header_t header;
    uint32_t length;
    uint32_t expected_crc;
    uint32_t offset;
    uint32_t count;
    uint32_t sector;
    uint32_t index;
    uint32_t actual_crc;

    if (W25Q_Init() != W25Q_OK)
    {
        boot_uart1_send_string("[BOOT] W25 init FAIL\r\n");
        return;
    }

    boot_uart1_send_string("[BOOT] RX READY\r\n");
    if ((boot_read_exact(begin, sizeof(begin)) == 0U) ||
        (boot_read_u32(begin) != BOOT_IMAGE_MAGIC))
    {
        boot_uart1_send_string("[BOOT] BEGIN timeout or bad magic\r\n");
        return;
    }

    length = boot_read_u32(begin + 4U);
    expected_crc = boot_read_u32(begin + 8U);
    if ((length < 8U) || (length > BOOT_MAX_IMAGE_SIZE))
    {
        boot_uart1_send_string("[BOOT] BIN length invalid\r\n");
        return;
    }

    boot_uart1_send_string("[BOOT] BIN length = ");
    boot_uart1_send_hex32(length);
    boot_uart1_send_string(", CRC32 = ");
    boot_uart1_send_hex32(expected_crc);
    boot_uart1_send_string("\r\n[BOOT] Erasing W25 image\r\n");

    /* 先擦掉旧头部，确保中断的接收绝不会留下旧的 READY 标志。 */
    if (W25Q_EraseSector(BOOT_W25_HEADER_ADDR) != W25Q_OK)
    {
        boot_uart1_send_string("[BOOT] Header erase FAIL\r\n");
        return;
    }
    for (sector = BOOT_W25_IMAGE_ADDR;
         sector < BOOT_W25_IMAGE_ADDR + length;
         sector += W25Q128_SECTOR_SIZE)
    {
        if (W25Q_EraseSector(sector) != W25Q_OK)
        {
            boot_uart1_send_string("[BOOT] Image erase FAIL\r\n");
            return;
        }
        if ((((sector - BOOT_W25_IMAGE_ADDR) / W25Q128_SECTOR_SIZE + 1U) % 32U == 0U) ||
            (sector + W25Q128_SECTOR_SIZE >= BOOT_W25_IMAGE_ADDR + length))
        {
            boot_uart1_send_string("[BOOT] W25 erase complete through ");
            boot_uart1_send_hex32(sector + W25Q128_SECTOR_SIZE);
            boot_uart1_send_string("\r\n");
        }
    }

    boot_uart1_send_string("[BOOT] W25 erase PASS; receiving BIN\r\n");
    boot_uart1_send_byte('K');
    for (offset = 0U; offset < length; offset += count)
    {
        count = length - offset;
        if (count > BOOT_PACKET_SIZE)
            count = BOOT_PACKET_SIZE;

        if ((boot_read_exact(packet, count) == 0U) ||
            (boot_read_exact(packet_crc_bytes, sizeof(packet_crc_bytes)) == 0U) ||
            (boot_crc32(packet, count) != boot_read_u32(packet_crc_bytes)))
        {
            boot_uart1_send_string("[BOOT] Packet CRC/timeout FAIL\r\n");
            return;
        }
        if ((W25Q_Write(BOOT_W25_IMAGE_ADDR + offset, packet, count) != W25Q_OK) ||
            (W25Q_Read(BOOT_W25_IMAGE_ADDR + offset, verify, count) != W25Q_OK))
        {
            boot_uart1_send_string("[BOOT] W25 write/read FAIL\r\n");
            return;
        }
        for (index = 0U; index < count; index++)
        {
            if (packet[index] != verify[index])
            {
                boot_uart1_send_string("[BOOT] W25 compare FAIL\r\n");
                return;
            }
        }
        if (((offset + count) % (32U * 1024U) == 0U) ||
            (offset + count == length))
        {
            boot_uart1_send_string("[BOOT] W25 verified bytes = ");
            boot_uart1_send_hex32(offset + count);
            boot_uart1_send_string("\r\n");
        }
        boot_uart1_send_byte('K');
    }

    if ((boot_w25_image_crc(length, &actual_crc) == 0U) ||
        (actual_crc != expected_crc) ||
        (W25Q_Read(BOOT_W25_IMAGE_ADDR, verify, 8U) != W25Q_OK) ||
        (boot_image_vector_valid(verify, length) == 0U))
    {
        boot_uart1_send_string("\r\n[BOOT] BIN verify FAIL\r\n");
        return;
    }

    boot_uart1_send_string("\r\n[BOOT] W25 full image CRC PASS\r\n");

    header.magic = BOOT_IMAGE_MAGIC;
    header.target = BOOT_APP_BASE;
    header.length = length;
    header.image_crc = expected_crc;
    header.header_crc = boot_crc32((const uint8_t *)&header, 16U);
    header.state = BOOT_STATE_READY;
    if (W25Q_Write(BOOT_W25_HEADER_ADDR, (const uint8_t *)&header,
                   sizeof(header)) != W25Q_OK)
    {
        boot_uart1_send_string("\r\n[BOOT] READY write FAIL\r\n");
        return;
    }

    boot_uart1_send_string("\r\n[BOOT] STAGED OK, resetting\r\n");
    NVIC_SystemReset();
}
