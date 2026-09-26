#ifndef WEATHERCLOCK_BOOT_UPDATE_H
#define WEATHERCLOCK_BOOT_UPDATE_H

/** Bootloader 对外部固件的安装结果。 */
typedef enum
{
    BOOT_UPDATE_NONE = 0,
    BOOT_UPDATE_INSTALLED,
    BOOT_UPDATE_FAILED
} boot_update_result_t;

/** 检查 W25Q128 的 READY 镜像，并在需要时安装到内部 APP 分区。 */
boot_update_result_t boot_update_install_pending(void);

#endif /* WEATHERCLOCK_BOOT_UPDATE_H */
