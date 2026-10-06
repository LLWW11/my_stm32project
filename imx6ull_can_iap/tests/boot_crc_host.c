/* 直接编译真实 Bootloader CRC 实现，保持主机测试与固件算法一致。
 * 使用工具目录内的源文件入口，避免 Windows 下外部源文件生成的对象路径包含中文目录。
 */
#include "../../bootloader/app/boot_crc.c"
