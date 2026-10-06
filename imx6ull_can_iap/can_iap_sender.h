#ifndef IMX6ULL_CAN_IAP_SENDER_H
#define IMX6ULL_CAN_IAP_SENDER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 与 STM32 当前协议一致：经典 CAN、标准帧、小端、每帧最多四字节正文。 */
#define IAP_START_ID 0x120U
#define IAP_DATA_ID 0x121U
#define IAP_END_ID 0x122U
#define IAP_REPLY_ID 0x123U
#define IAP_REPLY_TAG 0xA5U
#define IAP_APP_BASE 0x08010000U
#define IAP_IMAGE_MAX 0x000F0000U

typedef struct {
    uint16_t id;
    uint8_t len;
    uint8_t data[8];
} iap_frame_t;

/* 回调约定：send 返回 0 成功、-1 失败；receive 返回 1 帧、0 超时、-1 失败。
 * receive 必须在 timeout_ms 内返回；now_ms 使用单调时钟。
 */
typedef struct {
    void *context;
    int (*send)(void *context, const iap_frame_t *frame);
    int (*receive)(void *context, iap_frame_t *frame, unsigned timeout_ms);
    uint64_t (*now_ms)(void *context);
} iap_link_t;

typedef struct {
    uint8_t session;
    unsigned start_timeout_ms;
    unsigned data_timeout_ms;
    unsigned end_timeout_ms;
    unsigned data_retries;
    FILE *log;
} iap_options_t;

typedef enum {
    IAP_TRANSFER_READY = 0, /* 仅证明外部镜像 READY；安装结果需查看 Bootloader。 */
    IAP_TRANSFER_FAILED = 1,
    IAP_TRANSFER_UNCERTAIN = 2 /* END 发出但回执丢失，禁止直接宣称安装失败。 */
} iap_result_t;

/** 计算 BIN 全量 CRC-32/ISO-HDLC，与 boot_crc32 保持一致。 */
uint32_t iap_crc32(const uint8_t *data, size_t length);
/** 检查 BIN 长度、MSP 和复位入口，拒绝 HEX、Bootloader 或未重定位镜像。 */
int iap_validate_image(const uint8_t *image, size_t length, FILE *log);
/** 设置长 START 超时、逐帧 DATA 重传和一次 END 提交的默认参数。 */
void iap_options_default(iap_options_t *options);
/** 按 START 回复断点续传镜像；只对 DATA 自动重传，成功仅表示接收端 READY。 */
iap_result_t iap_transfer(iap_link_t *link, const uint8_t *image,
                          size_t length, const iap_options_t *options);
/** 打开 Linux CAN_RAW 套接字并建立回复过滤器，不修改接口位速率。 */
int iap_socketcan_open(iap_link_t *link, const char *interface_name);
/** 关闭套接字并释放 Linux 传输上下文。 */
void iap_socketcan_close(iap_link_t *link);

#endif
