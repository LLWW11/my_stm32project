#include "can_iap_sender.h"

#include <limits.h>
#include <string.h>

/** 将 32 位字段编码为协议小端字节。 */
static void put_u32(uint8_t *data, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i)
        data[i] = (uint8_t)(value >> (8U * i));
}

/** 读取协议小端 32 位字段。 */
static uint32_t get_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/** 读取 ACK 中的 24 位下一期望偏移。 */
static uint32_t get_u24(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16);
}

/** 计算与 STM32 相同的反射 CRC32，初值及末尾异或均为 0xFFFFFFFF。 */
uint32_t iap_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    size_t i;
    unsigned bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
    }
    return crc ^ 0xFFFFFFFFU;
}

/** 发送前检查长度与向量，规则对应 boot_image_vector_valid。 */
int iap_validate_image(const uint8_t *image, size_t length, FILE *log)
{
    uint32_t msp, reset, entry;
    if (!image || length < 8 || length > IAP_IMAGE_MAX) {
        if (log) fprintf(log, "Invalid BIN length: %lu (allowed 8..%u)\n",
                         (unsigned long)length, IAP_IMAGE_MAX);
        return -1;
    }
    msp = get_u32(image);
    reset = get_u32(image + 4);
    entry = reset & ~1U;
    if (msp < 0x20000000U || msp > 0x20020000U || (msp & 7U) ||
        !(reset & 1U) || entry < IAP_APP_BASE + 8U ||
        entry >= IAP_APP_BASE + (uint32_t)length) {
        if (log) fprintf(log, "Invalid vector: MSP=0x%08lX Reset=0x%08lX; "
                         "BIN must be linked at 0x08010000\n",
                         (unsigned long)msp, (unsigned long)reset);
        return -1;
    }
    return 0;
}

/** 设置保守等待参数，DATA 重传间隔小于 STM32 的三秒间隔限制。 */
void iap_options_default(iap_options_t *options)
{
    options->session = 1;
    options->start_timeout_ms = 180000;
    options->data_timeout_ms = 500;
    options->end_timeout_ms = 30000;
    options->data_retries = 3;
    options->log = stderr;
}

/** 将接收端错误码转换为简短诊断文本。 */
static const char *error_name(uint8_t code)
{
    static const char *const names[] = {
        "OK", "FRAME/state", "LENGTH/vector", "OFFSET", "SESSION",
        "FLASH", "CRC", "TIMEOUT"
    };
    return code < sizeof(names) / sizeof(names[0]) ? names[code] : "UNKNOWN";
}

/* 等待结果：1 确认，0 超时，-1 链路或回复格式异常，-2 接收端明确拒绝，2 请求重传。 */
/** 在固定截止时间内筛选会话、阶段和进度；迟到回复不能延长等待。 */
static int wait_reply(iap_link_t *link, uint16_t request, uint8_t session,
                      uint32_t offset, uint32_t next, unsigned timeout_ms, FILE *log,
                      uint32_t *start_offset)
{
    uint64_t deadline = link->now_ms(link->context) + timeout_ms;
    for (;;) {
        iap_frame_t reply;
        uint64_t now = link->now_ms(link->context);
        uint32_t progress;
        unsigned remaining;
        int result;
        if (now >= deadline) return 0;
        remaining = (unsigned)(deadline - now);
        result = link->receive(link->context, &reply, remaining);
        if (result <= 0) return result;
        if (reply.id != IAP_REPLY_ID || reply.len != 8) continue;
        /* 旧 APP 无阶段标签时不能安全确认 END，明确要求刷新 APP。 */
        if (reply.data[7] != IAP_REPLY_TAG) {
            if (log) fprintf(log, "Untagged ACK: flash the matching STM32 APP first\n");
            return -1;
        }
        if (reply.data[1] != session) continue;
        if (reply.data[6] == 0 && reply.data[0] == 1 && reply.data[5] == 7) {
            if (log) fprintf(log, "Receiver aborted on inter-frame TIMEOUT\n");
            return -2;
        }
        if (reply.data[6] != (uint8_t)request) continue;
        progress = get_u24(reply.data + 2);
        if (reply.data[0] == 0 && reply.data[5] == 0) {
            /* START 的 next 为 BIN 长度，允许未对齐断点，以接收端记录为准。 */
            if (request == IAP_START_ID && start_offset && progress <= next) {
                *start_offset = progress;
                return 1;
            }
            if (progress == next) return 1;
            if (request == IAP_DATA_ID && progress <= offset) continue;
            if (log) fprintf(log, "Unexpected ACK offset %lu, expected %lu\n",
                             (unsigned long)progress, (unsigned long)next);
            return -1;
        }
        if (reply.data[0] == 1) {
            if (request == IAP_DATA_ID && reply.data[5] == 3 && progress == offset)
                return 2;
            if (log) fprintf(log, "NACK: %s (%u), next=%lu\n",
                             error_name(reply.data[5]), reply.data[5],
                             (unsigned long)progress);
            return -2;
        }
        if (log) fprintf(log, "Malformed ACK status/error\n");
        return -1;
    }
}

/** START 获取接收端断点，逐帧续传 DATA，再执行一次 END 提交。 */
iap_result_t iap_transfer(iap_link_t *link, const uint8_t *image,
                          size_t length, const iap_options_t *options)
{
    iap_frame_t frame;
    uint32_t offset, crc;
    uint32_t report_at = 65536;
    int reply;
    if (!link || !options || !link->send || !link->receive || !link->now_ms ||
        options->session == 0 || !options->start_timeout_ms ||
        !options->data_timeout_ms || options->data_timeout_ms > 1000 ||
        !options->end_timeout_ms || options->data_retries > 10 ||
        iap_validate_image(image, length, options->log) != 0)
        return IAP_TRANSFER_FAILED;

    crc = iap_crc32(image, length);
    memset(&frame, 0, sizeof(frame));
    frame.id = IAP_START_ID;
    frame.len = 8;
    put_u32(frame.data, (uint32_t)length);
    put_u32(frame.data + 4, crc);
    if (options->log) fprintf(options->log, "START len=%lu CRC32=0x%08lX; "
                             "waiting for resume offset or erase (up to %u ms)\n",
                             (unsigned long)length, (unsigned long)crc,
                             options->start_timeout_ms);
    /* START 不重发：擦除过程中重发可能排队并导致再次擦除。 */
    if (link->send(link->context, &frame) != 0) return IAP_TRANSFER_FAILED;
    offset = 0;
    reply = wait_reply(link, IAP_START_ID, 0, 0, (uint32_t)length,
                       options->start_timeout_ms, options->log, &offset);
    if (reply != 1) {
        if (options->log) fprintf(options->log, "START not confirmed; no DATA sent\n");
        return IAP_TRANSFER_FAILED;
    }
    if (options->log) fprintf(options->log, "%s offset=%lu/%lu (%.1f%%)\n",
                             offset ? "RESUME" : "NEW transfer",
                             (unsigned long)offset, (unsigned long)length,
                             100.0 * offset / length);
    report_at = (offset / 65536U + 1U) * 65536U;

    /* 收齐正文后断电：重发末尾分片绑定新会话，接收端只确认而不改写。 */
    if (offset == length) offset = (uint32_t)length - 1U;

    for (; offset < length;) {
        unsigned count = (unsigned)(length - offset);
        unsigned attempt;
        if (count > 4) count = 4;
        frame.id = IAP_DATA_ID;
        frame.len = (uint8_t)(4 + count);
        frame.data[0] = options->session;
        frame.data[1] = (uint8_t)offset;
        frame.data[2] = (uint8_t)(offset >> 8);
        frame.data[3] = (uint8_t)(offset >> 16);
        memcpy(frame.data + 4, image + offset, count);
        for (attempt = 0; attempt <= options->data_retries; ++attempt) {
            if (link->send(link->context, &frame) != 0) {
                reply = -1;
                break;
            }
            reply = wait_reply(link, IAP_DATA_ID, options->session, offset,
                               offset + count, options->data_timeout_ms, options->log, NULL);
            if (reply == 1 || reply < 0) break;
            if (options->log && attempt < options->data_retries)
                fprintf(options->log, "DATA retry at %lu (%u/%u)\n",
                                     (unsigned long)offset, attempt + 1,
                                     options->data_retries);
        }
        if (reply != 1) {
            if (options->log) fprintf(options->log, "DATA stopped at %lu; restore link, "
                                     "wait for receiver IDLE (at least 3 s), then rerun "
                                     "with the same BIN to resume\n", (unsigned long)offset);
            return IAP_TRANSFER_FAILED;
        }
        offset += count;
        if (offset >= report_at || offset == length) {
            if (options->log) fprintf(options->log, "DATA %lu/%lu (%.1f%%)\n",
                                     (unsigned long)offset, (unsigned long)length,
                                     100.0 * offset / length);
            report_at += 65536;
        }
    }

    frame.id = IAP_END_ID;
    frame.len = 5;
    frame.data[0] = options->session;
    put_u32(frame.data + 1, (uint32_t)length);
    if (options->log) fprintf(options->log, "END: waiting for readback CRC and READY\n");
    if (link->send(link->context, &frame) != 0) return IAP_TRANSFER_UNCERTAIN;
    reply = wait_reply(link, IAP_END_ID, options->session, offset, offset,
                       options->end_timeout_ms, options->log, NULL);
    if (reply == 1) {
        if (options->log) fprintf(options->log, "Image READY; verify BOOT Internal CRC "
                                 "PASS, Install PASS and the new APP on serial console\n");
        return IAP_TRANSFER_READY;
    }
    /* END 之后接收端可能复位。没有确认不能盲目重发 START 或 END。 */
    if (options->log) fprintf(options->log, "END unconfirmed; inspect serial log "
                             "before restarting transfer\n");
    return reply == -2 ? IAP_TRANSFER_FAILED : IAP_TRANSFER_UNCERTAIN;
}
