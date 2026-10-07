# weather-clock-FreeRTOS

天气时钟项目代码，用的是正点原子stm32f407zgt6开发板，所以有点不一样，使用标准库➕FreeRTOS

这个是使用了lvgl库的版本，很多东西为了和非lvgl库兼容，大量使用了宏，同时增加了二级页面，只不过显示的东西有限；

已实现 CAN IAP：i.MX6ULL 使用纯 C / SocketCAN 工具发送 APP BIN，STM32 分片写入 W25Q128、校验 CRC32 并提交 READY，复位后由 Bootloader 安装至内部 Flash。

AT24C02 每约 4 KiB 或正文收齐时保存接收检查点。传输中断后重新运行同一 BIN，发送端根据 START 返回的偏移继续；2026-10-06 用户反馈，删除 I2C1 初始化末尾的 `IIC1_BusRecover()` 调用后，STM32 RESET 中断续传已正常。实际断电和安装阶段恢复仍需单独验证。

操作与验证边界见 [i.MX6ULL 发送工具说明](imx6ull_can_iap/README.md)、[快速操作说明](imx6ull_can_iap/简单说明.md) 和 [W25Q128 镜像安装说明](bootloader/W25Q128镜像安装说明.md)。
