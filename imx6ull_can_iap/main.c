#include "can_iap_sender.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** 输出命令行参数与返回码说明。 */
static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s --interface can0 --file app.bin [options]\n"
            "  --dry-run                 validate BIN and CRC; never open CAN\n"
            "  --session 1..255          default: random per invocation\n"
            "  --start-timeout-ms N      default: 180000 (erase entire W25 image)\n"
            "  --data-timeout-ms 1..1000 default: 500 (receiver gap: 3000 ms)\n"
            "  --end-timeout-ms N        default: 30000 (readback CRC + READY)\n"
            "  --retries 0..10           default: 3; DATA only\n"
            "Exit: 0 READY/dry-run OK, 1 failed, 2 END outcome unconfirmed\n", program);
}

/** 严格解析有上界的非负十进制参数，拒绝负数、溢出和尾部字符。 */
static int parse_number(const char *text, unsigned maximum, unsigned *value)
{
    char *end;
    unsigned long parsed;
    if (!text[0] || text[0] == '-' || text[0] == '+') return -1;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno || *end || parsed > maximum) return -1;
    *value = (unsigned)parsed;
    return 0;
}

/** 有界读取原始 BIN 到主机内存，STM32 端仍按四字节流式落盘。 */
static uint8_t *load_image(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *image;
    if (!file) { perror(path); return NULL; }
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 8 || size > (long)IAP_IMAGE_MAX ||
        fseek(file, 0, SEEK_SET)) {
        fprintf(stderr, "BIN size must be 8..%u bytes\n", IAP_IMAGE_MAX);
        fclose(file);
        return NULL;
    }
    image = malloc((size_t)size);
    if (!image) { perror("malloc BIN"); fclose(file); return NULL; }
    if (fread(image, 1, (size_t)size, file) != (size_t)size || ferror(file)) {
        fprintf(stderr, "Cannot read complete BIN\n");
        free(image);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *length = (size_t)size;
    return image;
}

/** 解析命令、预检镜像，再通过 Linux SocketCAN 执行传输。 */
int main(int argc, char **argv)
{
    iap_options_t options;
    iap_link_t link;
    const char *interface_name = "can0";
    const char *path = NULL;
    int dry_run = 0;
    int i;
    uint8_t *image;
    size_t length;
    iap_result_t result;
    iap_options_default(&options);
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    options.session = (uint8_t)(1 + rand() % 255); /* 会话用于区分回复，不承担认证。 */
    for (i = 1; i < argc; ++i) {
        unsigned value;
        unsigned maximum = 600000;
        const char *name = argv[i];
        if (!strcmp(name, "--help")) { usage(argv[0]); return 0; }
        if (!strcmp(name, "--dry-run")) { dry_run = 1; continue; }
        if (++i >= argc) { usage(argv[0]); return 1; }
        if (!strcmp(name, "--interface")) { interface_name = argv[i]; continue; }
        if (!strcmp(name, "--file")) { path = argv[i]; continue; }
        if (!strcmp(name, "--session")) maximum = 255;
        else if (!strcmp(name, "--data-timeout-ms")) maximum = 1000;
        else if (!strcmp(name, "--retries")) maximum = 10;
        else if (strcmp(name, "--start-timeout-ms") && strcmp(name, "--end-timeout-ms")) {
            fprintf(stderr, "Unknown option: %s\n", name);
            return 1;
        }
        if (parse_number(argv[i], maximum, &value) || (!value && strcmp(name, "--retries"))) {
            fprintf(stderr, "Invalid value for %s\n", name);
            return 1;
        }
        if (!strcmp(name, "--session")) options.session = (uint8_t)value;
        else if (!strcmp(name, "--data-timeout-ms")) options.data_timeout_ms = value;
        else if (!strcmp(name, "--start-timeout-ms")) options.start_timeout_ms = value;
        else if (!strcmp(name, "--end-timeout-ms")) options.end_timeout_ms = value;
        else options.data_retries = value;
    }
    if (!path) { usage(argv[0]); return 1; }
    image = load_image(path, &length);
    if (!image) return 1;
    if (iap_validate_image(image, length, stderr)) { free(image); return 1; }
    fprintf(stderr, "BIN=%s length=%lu CRC32=0x%08lX session=%u\n", path,
            (unsigned long)length, (unsigned long)iap_crc32(image, length), options.session);
    if (dry_run) { free(image); return 0; }
    if (iap_socketcan_open(&link, interface_name)) { free(image); return 1; }
    result = iap_transfer(&link, image, length, &options);
    iap_socketcan_close(&link);
    free(image);
    return (int)result;
}
