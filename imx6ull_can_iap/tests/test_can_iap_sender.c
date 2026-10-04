#include "can_iap_sender.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* 直接编译实际 APP 处理函数，避免另写一个与实现一致的假接收状态机。 */
#include "../../../app/CAN_IAP_Task.c"
#include "../../../bootloader/app/boot_update.h"

enum scenario {
    NORMAL, LOST_DATA, LOST_DATA_ACK, LOST_START_ACK, LOST_END_ACK,
    STALE_DATA_ACK_AT_END, WRONG_SESSION_ACK, CRC_FAILURE, FLASH_FAILURE,
    HEADER_FAILURE, ALL_DATA_LOST, UNTAGGED_ACK, OFFSET_RETRY, OLD_OFFSET_ACK,
    FUTURE_OFFSET_ACK, WRONG_PHASE_ONLY
};

static uint8_t flash_data[BOOT_IMAGE_W25_BODY + BOOT_IMAGE_APP_MAX_SIZE];
static uint8_t image[37]; /* 非四字节倍数验证最后一帧 DLC。 */
static iap_ctx_t receiver;
static iap_frame_t replies[8];
static unsigned reply_count, reply_index;
static uint64_t virtual_clock;
static unsigned start_count, end_count, data_count, write_count;
static unsigned first_data_count;
static enum scenario mode;
static int corrupt_header;

/** 构造测试镜像和 START 字段的小端整数。 */
static void put_u32(uint8_t *data, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) data[i] = (uint8_t)(value >> (8U * i));
}

/** 用内存模拟 NOR 写入，只允许 1 到 0，越界或模拟故障返回错误。 */
w25q_status_t W25Q_Write(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t i;
    if (address + length > sizeof(flash_data) || mode == FLASH_FAILURE)
        return W25Q_ERROR_SPI_TIMEOUT;
    for (i = 0; i < length; ++i) {
        if ((flash_data[address + i] & data[i]) != data[i]) return W25Q_ERROR_WRITE_ENABLE;
        flash_data[address + i] &= data[i];
    }
    ++write_count;
    if (mode == HEADER_FAILURE && address == BOOT_IMAGE_W25_HEADER) corrupt_header = 1;
    return W25Q_OK;
}

/** 模拟读回，可在提交 READY 之前注入元数据位损坏。 */
w25q_status_t W25Q_Read(uint32_t address, uint8_t *data, uint32_t length)
{
    if (address + length > sizeof(flash_data)) return W25Q_ERROR_OUT_OF_RANGE;
    memcpy(data, flash_data + address, length);
    if (corrupt_header && address == BOOT_IMAGE_W25_HEADER) data[0] ^= 1;
    return W25Q_OK;
}

/** 按四 KiB 扇区模拟擦除，不接触任何硬件。 */
w25q_status_t W25Q_EraseSector(uint32_t address)
{
    address &= ~(uint32_t)(W25Q128_SECTOR_SIZE - 1);
    assert(address + W25Q128_SECTOR_SIZE <= sizeof(flash_data));
    memset(flash_data + address, 0xFF, W25Q128_SECTOR_SIZE);
    return W25Q_OK;
}

/** 模拟已初始化的 W25Q128。 */
w25q_status_t W25Q_Init(void) { return W25Q_OK; }
/** 模拟正确的 JEDEC 标识。 */
w25q_status_t W25Q_ReadId(uint32_t *id) { *id = W25Q128_JEDEC_ID; return W25Q_OK; }
/** 测试只调用协议处理函数，此处硬件初始化固定成功。 */
bool can_test_init(bool loopback) { (void)loopback; return true; }
/** 测试不涉及硬件滤波器。 */
void can_test_enable_probe_filter(void) { }
/** 记录真正的 iap_reply 输出到虚拟接收队列。 */
bool can_test_send(uint16_t id, const uint8_t *data, uint8_t len, TickType_t timeout)
{
    iap_frame_t *reply;
    (void)timeout;
    assert(reply_count < 8);
    reply = &replies[reply_count++];
    reply->id = id;
    reply->len = len;
    memcpy(reply->data, data, len);
    return true;
}
/** 测试不执行 can_iap_task 的无限任务循环。 */
bool can_test_receive(CanRxMsg *rx, TickType_t timeout)
{ (void)rx; (void)timeout; return false; }
/** 主机测试占位，不删除线程。 */
void vTaskDelete(void *task) { (void)task; }
/** 主机测试占位，不进行实际休眠。 */
void vTaskDelay(TickType_t ticks) { (void)ticks; }
/** 若测试误进入复位路径，立即报告错误。 */
void NVIC_SystemReset(void) { assert(0 && "Unexpected hardware reset"); }

/** 从 sender 回调进入真实 APP START/DATA/END，并注入丢包及迟到回执。 */
static int simulated_send(void *context, const iap_frame_t *frame)
{
    iap_err_t error;
    (void)context;
    reply_count = reply_index = 0;
    if (frame->id == IAP_START_ID) {
        ++start_count;
        error = iap_handle_start(frame->data, frame->len, &receiver);
    } else if (frame->id == IAP_DATA_ID) {
        ++data_count;
        if (IAP_Get_u24_le(frame->data + 1) == 0) ++first_data_count;
        if (mode == ALL_DATA_LOST || (mode == LOST_DATA && first_data_count == 1)) return 0;
        if (mode == OFFSET_RETRY && first_data_count == 1) {
            iap_reply(1, frame->data[0], 0, IAP_ERR_OFFSET, (uint8_t)frame->id);
            return 0;
        }
        error = iap_handle_data(frame->data, frame->len, &receiver);
    } else {
        assert(frame->id == IAP_END_ID);
        ++end_count;
        if (mode == CRC_FAILURE) flash_data[BOOT_IMAGE_W25_BODY + 8] ^= 1;
        if (mode == STALE_DATA_ACK_AT_END)
            iap_reply(0, receiver.session, receiver.expected, IAP_ERR_OK, 0x21);
        error = iap_handle_end(frame->data, frame->len, &receiver);
    }
    if (frame->id == IAP_DATA_ID && mode == WRONG_SESSION_ACK)
        iap_reply(0, (uint8_t)(frame->data[0] + 1), receiver.expected, IAP_ERR_OK, 0x21);
    if (frame->id == IAP_DATA_ID && mode == OLD_OFFSET_ACK)
        iap_reply(0, receiver.session, IAP_Get_u24_le(frame->data + 1), IAP_ERR_OK, 0x21);
    iap_reply(error == IAP_ERR_OK ? 0 : 1,
              frame->id == IAP_START_ID ? 0 : frame->data[0], receiver.expected,
              error, (uint8_t)frame->id);
    if ((mode == LOST_START_ACK && frame->id == IAP_START_ID) ||
        (mode == LOST_END_ACK && frame->id == IAP_END_ID) ||
        (mode == LOST_DATA_ACK && frame->id == IAP_DATA_ID && first_data_count == 1))
        reply_count = 0;
    if (mode == UNTAGGED_ACK) replies[reply_count - 1].data[7] = 0;
    if (mode == FUTURE_OFFSET_ACK && frame->id == IAP_DATA_ID)
        ++replies[reply_count - 1].data[2];
    if (mode == WRONG_PHASE_ONLY && frame->id == IAP_END_ID)
        replies[reply_count - 1].data[6] = 0x21;
    return 0;
}

/** 虚拟时钟立即推进到超时，不用真实休眠运行丢包测试。 */
static int simulated_receive(void *context, iap_frame_t *frame, unsigned timeout_ms)
{
    (void)context;
    if (reply_index < reply_count) {
        *frame = replies[reply_index++];
        ++virtual_clock;
        return 1;
    }
    virtual_clock += timeout_ms;
    return 0;
}

/** 返回模拟链路单调时钟。 */
static uint64_t simulated_now(void *context) { (void)context; return virtual_clock; }

/** 初始化一轮独立传输并校验成功时的真实外部镜像头与正文。 */
static void run_case(enum scenario scenario, iap_result_t expected)
{
    iap_options_t options;
    iap_link_t link = {NULL, simulated_send, simulated_receive, simulated_now};
    boot_image_header_t header;
    memset(flash_data, 0xFF, sizeof(flash_data));
    memset(&receiver, 0, sizeof(receiver));
    mode = scenario;
    reply_count = reply_index = start_count = end_count = data_count = 0;
    write_count = first_data_count = 0;
    virtual_clock = 0;
    corrupt_header = 0;
    iap_options_default(&options);
    options.session = 73;
    options.log = NULL;
    assert(iap_transfer(&link, image, sizeof(image), &options) == expected);
    assert(start_count == 1); /* 所有异常路径都不能自动重复 START。 */
    if (expected == IAP_TRANSFER_READY) {
        memcpy(&header, flash_data, sizeof(header));
        assert(sizeof(header) == 24);
        assert(header.state == BOOT_IMAGE_STATE_READY);
        assert(header.length == sizeof(image));
        assert(header.image_crc == iap_crc32(image, sizeof(image)));
        assert(header.header_crc == iap_crc32((uint8_t *)&header, 16));
        assert(memcmp(flash_data + BOOT_IMAGE_W25_BODY, image, sizeof(image)) == 0);
        assert(end_count == 1);
    }
    if (scenario == LOST_DATA_ACK) assert(write_count == 12); /* 十帧 DATA 加两次头部写入。 */
    if (scenario == LOST_START_ACK) assert(data_count == 0 && end_count == 0);
    if (scenario == HEADER_FAILURE || scenario == CRC_FAILURE || scenario == FLASH_FAILURE)
        assert(flash_data[offsetof(boot_image_header_t, state)] == 0xFF);
}

/** 验证接收端会话绑定、重复 START、越界和重复 DATA 不再次写入。 */
static void test_receiver_boundaries(void)
{
    iap_ctx_t ctx = {0};
    uint8_t start[8], data[8] = {12, 4, 0, 0, 1, 2, 3, 4};
    unsigned previous_writes;
    mode = NORMAL;
    put_u32(start, sizeof(image));
    put_u32(start + 4, iap_crc32(image, sizeof(image)));
    assert(iap_handle_start(start, 8, &ctx) == IAP_ERR_OK);
    assert(iap_handle_start(start, 8, &ctx) == IAP_ERR_FRAME);
    assert(iap_handle_data(data, 8, &ctx) == IAP_ERR_OFFSET);
    assert(!ctx.session_valid);
    data[1] = 0;
    assert(iap_handle_data(data, 8, &ctx) == IAP_ERR_OK);
    previous_writes = write_count;
    assert(iap_handle_data(data, 8, &ctx) == IAP_ERR_OK && write_count == previous_writes);
    data[0] = 13;
    assert(iap_handle_data(data, 8, &ctx) == IAP_ERR_SESSION);
    data[0] = 12;
    data[1] = 36;
    assert(iap_handle_data(data, 8, &ctx) == IAP_ERR_LENGTH);
}

/** 传输最大镜像，跨过 16 位偏移边界并核对全量写入及 READY。 */
static void test_maximum_image(void)
{
    uint8_t *large = malloc(IAP_IMAGE_MAX);
    iap_options_t options;
    iap_link_t link = {NULL, simulated_send, simulated_receive, simulated_now};
    boot_image_header_t header;
    uint32_t i;
    assert(large);
    for (i = 0; i < IAP_IMAGE_MAX; ++i) large[i] = (uint8_t)(i ^ (i >> 8));
    put_u32(large, 0x20020000);
    put_u32(large + 4, IAP_APP_BASE + 9);
    mode = NORMAL;
    corrupt_header = 0;
    memset(flash_data, 0xFF, sizeof(flash_data));
    memset(&receiver, 0, sizeof(receiver));
    start_count = end_count = data_count = first_data_count = write_count = 0;
    virtual_clock = 0;
    iap_options_default(&options);
    options.log = NULL;
    assert(iap_transfer(&link, large, IAP_IMAGE_MAX, &options) == IAP_TRANSFER_READY);
    memcpy(&header, flash_data, sizeof(header));
    assert(header.state == BOOT_IMAGE_STATE_READY && header.length == IAP_IMAGE_MAX);
    assert(start_count == 1 && end_count == 1 && data_count == IAP_IMAGE_MAX / 4);
    assert(memcmp(flash_data + BOOT_IMAGE_W25_BODY, large, IAP_IMAGE_MAX) == 0);
    free(large);
}

/** 执行正常、丢包、迟到 ACK、错误会话、CRC 与 Flash 故障的协议测试。 */
int main(void)
{
    unsigned i;
    for (i = 0; i < sizeof(image); ++i) image[i] = (uint8_t)i;
    put_u32(image, 0x20010000);
    put_u32(image + 4, IAP_APP_BASE + 9);
    assert(iap_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926);
    assert(iap_validate_image(image, sizeof(image), NULL) == 0);
    assert(iap_validate_image(image, 7, NULL) != 0);
    assert(iap_validate_image(image, IAP_IMAGE_MAX + 1U, NULL) != 0);
    image[4] &= 0xFE;
    assert(iap_validate_image(image, sizeof(image), NULL) != 0);
    image[4] |= 1;
    run_case(NORMAL, IAP_TRANSFER_READY);
    run_case(LOST_DATA, IAP_TRANSFER_READY);
    run_case(LOST_DATA_ACK, IAP_TRANSFER_READY);
    run_case(LOST_START_ACK, IAP_TRANSFER_FAILED);
    run_case(LOST_END_ACK, IAP_TRANSFER_UNCERTAIN);
    run_case(STALE_DATA_ACK_AT_END, IAP_TRANSFER_READY);
    run_case(WRONG_SESSION_ACK, IAP_TRANSFER_READY);
    run_case(CRC_FAILURE, IAP_TRANSFER_FAILED);
    run_case(FLASH_FAILURE, IAP_TRANSFER_FAILED);
    run_case(HEADER_FAILURE, IAP_TRANSFER_FAILED);
    run_case(ALL_DATA_LOST, IAP_TRANSFER_FAILED);
    run_case(UNTAGGED_ACK, IAP_TRANSFER_FAILED);
    run_case(OFFSET_RETRY, IAP_TRANSFER_READY);
    run_case(OLD_OFFSET_ACK, IAP_TRANSFER_READY);
    run_case(FUTURE_OFFSET_ACK, IAP_TRANSFER_FAILED);
    run_case(WRONG_PHASE_ONLY, IAP_TRANSFER_UNCERTAIN);
    test_receiver_boundaries();
    test_maximum_image();
    puts("PASS: 16 fault scenarios, 960 KiB transfer, vector/CRC and actual APP boundaries");
    return 0;
}
