#ifndef WEATHERCLOCK_BOOT_IMAGE_DESC_H
#define WEATHERCLOCK_BOOT_IMAGE_DESC_H

#include <stdint.h>

/* ========== 镜像格式 bootloader 与 APP 两个工程共用 ==========
 * 本文件定义 W25Q128 镜像区的内存布局。修改布局前先读完下面两条规则：
 * 1. 新增受 header_crc 保护的字段（如 version），必须插在 image_crc 与 header_crc
 *    之间；CRC 覆盖长度用 offsetof(boot_image_header_t, header_crc) 计算，
 *    state 偏移用 offsetof(boot_image_header_t, state) 计算，两侧自动跟随。
 * 2. state 必须保持最后一个字段：
 *      擦除态 0xFFFFFFFF → READY 0xFFFFFFFE → DONE 0xFFFFFFFC，只清位不擦除
 */

// ---------- 内部 Flash 分区 ----------
#define BOOT_IMAGE_APP_BASE     0x08010000U // APP 起始地址
#define BOOT_IMAGE_APP_END      0x08100000U // APP 结束地址，不含
#define BOOT_IMAGE_APP_MAX_SIZE 0x000F0000U // APP 最大 960 KiB

// ---------- W25Q128 镜像区布局 ----------  //
#define BOOT_IMAGE_W25_HEADER 0x00000000U // 头部，首个 4 KiB 扇区
#define BOOT_IMAGE_W25_BODY   0x00001000U // BIN 正文起始

// ---------- 镜像标识与状态 ----------
#define BOOT_IMAGE_MAGIC       0x31505557U // 字节序为 "WUP1"
#define BOOT_IMAGE_STATE_READY 0xFFFFFFFEU // 待安装
#define BOOT_IMAGE_STATE_DONE  0xFFFFFFFCU // 已安装

// ---------- 镜像头结构：小端、全 32 位成员、无填充，共 24 字节 ----------
/*
布局：   + 0 magic |
        + 4 target |
        + 8 length |
        +12 image_crc * +16 header_crc（覆盖前 16 字节）|
        +20 state（最后写入，位翻转提交）
*/
typedef struct
{
    uint32_t magic;  // BOOT_IMAGE_MAGIC
    uint32_t target; // 固定为 BOOT_IMAGE_APP_BASE
    uint32_t length; // BIN 字节长度
    // uint32_t image_version;// 版本号，先占个坑
    uint32_t image_crc;  // BIN 全量 CRC-32/ISO-HDLC
    uint32_t header_crc; // 本结构从头开始到本字段之前的 CRC32
    uint32_t state;      // 擦除态 0xFFFFFFFF -> READY -> DONE
} boot_image_header_t;

// 编译期防护：加了非 32 位成员或字段顺序改动导致填充/长度变化时，直接编译失败
typedef char boot_image_header_size_check[(sizeof(boot_image_header_t) == 24U) ? 1 : -1];

#endif // WEATHERCLOCK_BOOT_IMAGE_DESC_H
