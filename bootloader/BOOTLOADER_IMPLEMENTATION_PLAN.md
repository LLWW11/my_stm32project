# WeatherClock Bootloader 详细实施计划

> 当前实现已取消串口接收固件：USART1 仅输出日志，Bootloader 只安装 W25Q128 中已有的 READY 镜像。下文的串口升级内容属于早期规划，当前镜像格式与操作边界见 [W25Q128 镜像安装说明](W25Q128镜像安装说明.md)。

## 1. 文档目的

本文档用于指导 STM32F407ZG 天气时钟项目从“最小串口启动程序”逐步演进为一个可用、可恢复、可扩展的 Bootloader。

实施时必须遵循以下原则：

- 每个阶段都能单独编译、烧录和验证。
- 先完成 APP 地址迁移与可靠跳转，再实现 Flash 擦写和升级协议。
- Bootloader 永远不能擦除或覆盖自身所在的 Flash 扇区。
- 任何升级失败或掉电都不能导致设备失去再次升级的入口。
- 编译成功、下载成功和上板运行成功必须分别记录，不能相互替代。
- 新增函数必须有用途说明、参数说明和关键步骤注释。

## 2. 当前基线

### 2.1 已完成内容

- Bootloader 链接地址为 `0x08000000`。
- Bootloader 最大链接空间为 `0x10000`，即 64 KiB。
- 使用 USART1，PA9 为 TX，PA10 为 RX。
- 串口参数为 115200、8 数据位、无校验、1 停止位。
- 上电或复位后打印 `Bootloader start`。
- 用户已经上板烧录并观察到启动字符串。
- Keil ARMCC5 工程可以生成 Bootloader HEX。

### 2.2 当前功能边界

当前 Bootloader 只完成了以下流程：

```text
复位
  ↓
启动文件初始化栈和运行环境
  ↓
SystemInit 初始化系统
  ↓
进入 main
  ↓
初始化 USART1
  ↓
打印 Bootloader start
  ↓
无限循环
```

当前尚未实现：

- APP 地址迁移。
- APP 向量表合法性检查。
- APP 跳转。
- 强制进入 Bootloader 的按键或软件标志。
- 串口接收升级数据。
- 内部 Flash 擦除和写入。
- 固件 CRC 校验。
- W25Q128 固件暂存和 OTA。

## 3. 总体目标

### 3.1 第一目标：可跳转 Bootloader

设备复位后先进入 Bootloader，检查 APP 是否有效；APP 有效时跳转到天气时钟 APP，APP 无效时停留在 Bootloader。

### 3.2 第二目标：串口 IAP Bootloader

Bootloader 能通过 USART1 接收 APP 固件，安全擦除 APP 分区、分包写入、回读并进行 CRC 校验，然后复位进入新 APP。

### 3.3 第三目标：W25Q128 OTA Bootloader

天气时钟 APP 通过网络把候选固件下载到 W25Q128，Bootloader 在复位后验证候选固件并把它写入内部 Flash。

## 4. Flash 分区

STM32F407ZG 内部 Flash 容量为 1 MiB，地址范围为 `0x08000000` 至 `0x080FFFFF`。

### 4.1 计划分区

| 区域 | 起始地址 | 结束地址 | 大小 | 对应扇区 |
|---|---:|---:|---:|---|
| Bootloader | `0x08000000` | `0x0800FFFF` | 64 KiB | Sector 0～3 |
| APP | `0x08010000` | `0x080FFFFF` | 960 KiB | Sector 4～11 |

### 4.2 STM32F407 扇区边界

| 扇区 | 起始地址 | 大小 | 用途 |
|---|---:|---:|---|
| Sector 0 | `0x08000000` | 16 KiB | Bootloader |
| Sector 1 | `0x08004000` | 16 KiB | Bootloader |
| Sector 2 | `0x08008000` | 16 KiB | Bootloader |
| Sector 3 | `0x0800C000` | 16 KiB | Bootloader |
| Sector 4 | `0x08010000` | 64 KiB | APP |
| Sector 5 | `0x08020000` | 128 KiB | APP |
| Sector 6 | `0x08040000` | 128 KiB | APP |
| Sector 7 | `0x08060000` | 128 KiB | APP |
| Sector 8 | `0x08080000` | 128 KiB | APP |
| Sector 9 | `0x080A0000` | 128 KiB | APP |
| Sector 10 | `0x080C0000` | 128 KiB | APP |
| Sector 11 | `0x080E0000` | 128 KiB | APP |

### 4.3 强制保护规则

- Bootloader 只允许擦除 Sector 4～11。
- Bootloader 只允许写入 `0x08010000` 至 `0x080FFFFF`。
- 接收到的写入地址和长度必须同时检查，防止地址加长度后越界。
- 不允许使用全片擦除作为正常升级手段。
- APP 固件长度不得超过 `0xF0000`。

## 5. 推荐代码结构

后续功能不应全部堆放在 `main.c` 中，建议逐步调整为：

```text
bootloader/app/
├── main.c
├── boot_config.h
├── boot_app.c
├── boot_app.h
├── boot_uart.c
├── boot_uart.h
├── boot_flash.c
├── boot_flash.h
├── boot_crc.c
├── boot_crc.h
├── boot_protocol.c
└── boot_protocol.h
```

各文件职责如下：

| 文件 | 职责 |
|---|---|
| `main.c` | Bootloader 主状态流程和模块调用 |
| `boot_config.h` | APP 地址、分区大小、串口参数、超时时间和版本号 |
| `boot_app.c/.h` | APP 向量检查、中断清理和安全跳转 |
| `boot_uart.c/.h` | USART1 初始化、阻塞收发、超时接收 |
| `boot_flash.c/.h` | APP 扇区擦除、数据写入和回读检查 |
| `boot_crc.c/.h` | 数据包 CRC16 和完整镜像 CRC32 |
| `boot_protocol.c/.h` | 升级帧解析、状态机、命令处理和应答 |

## 6. 阶段一：固定并验证当前基线

### 6.1 目标

在开始修改 APP 和跳转逻辑前，确保当前 Bootloader 工程配置可重复构建。

### 6.2 工作项

- 确认 EIDE 使用 ARM Compiler 5。
- 确认 CPU 为 `Cortex-M4`，硬件浮点为单精度。
- 确认 Bootloader IROM 为 `0x08000000 / 0x10000`。
- 确认 SRAM 为 `0x20000000 / 0x20000`。
- 确认 EIDE 的存储布局与 `weatherClock_bootloader.sct` 一致。
- 明确使用“自定义分散加载文件”还是“EIDE 自动生成分散加载文件”，避免两套配置不一致。
- 构建后检查 map 和 HEX 的真实地址，而不是只检查设置页面。

### 6.3 验收标准

- ARMCC5 构建为 0 错误、0 警告。
- HEX 最低地址为 `0x08000000`。
- HEX 最高地址小于 `0x08010000`。
- 单次按下 RESET 只新增一行 `Bootloader start`。

## 7. 阶段二：天气时钟 APP 地址迁移

### 7.1 目标

让现有 FreeRTOS 和 LVGL 天气时钟 APP 能够链接并运行在 `0x08010000`。

### 7.2 修改项

#### 7.2.1 修改 APP IROM

将 APP 目标配置修改为：

```text
IROM1 Start = 0x08010000
IROM1 Size  = 0x000F0000
```

如果 APP 使用自定义分散加载文件，也必须同步修改加载区和执行区。

#### 7.2.2 修改 APP 向量表偏移

将 APP 的 `firmware/cmsis/device/system_stm32f4xx.c` 中：

```text
VECT_TAB_OFFSET = 0x00
```

修改为：

```text
VECT_TAB_OFFSET = 0x10000
```

APP 的 `SystemInit()` 最终应把 `SCB->VTOR` 设置为 `0x08010000`。

#### 7.2.3 增加临时 APP 启动标志

在联调阶段，可以在 APP 启动初期临时打印 `APP start`，用于区分 Bootloader 输出和 APP 输出。验证完成后再决定是否保留。

### 7.3 构建检查

- 检查 APP HEX 最低地址不小于 `0x08010000`。
- 检查 APP HEX 最高地址小于 `0x08100000`。
- 检查 map 文件中 RESET 段位于 `0x08010000`。
- 检查 APP 固件总长度不超过 `0xF0000`。
- 检查 Bootloader HEX 与 APP HEX 没有地址重叠。

### 7.4 烧录注意事项

- 分别烧录 Bootloader HEX 和 APP HEX。
- 烧录 APP 时不能使用会擦除全片 Flash 的配置。
- 重新烧录 Bootloader 后，需要确认 APP 扇区是否仍然保留。
- 重新烧录 APP 后，需要确认 Bootloader 扇区是否仍然保留。

### 7.5 验收标准

- APP 能成功构建。
- APP 向量表位于 `0x08010000`。
- APP 中断向量偏移为 `0x10000`。
- 通过调试器从 APP 入口运行时，FreeRTOS、LVGL、SysTick 和外设中断正常。

## 8. 阶段三：APP 合法性检查

### 8.1 APP 向量表含义

APP 分区开头的两个 32 位数据分别是：

| 地址 | 内容 |
|---|---|
| `0x08010000` | APP 初始主栈指针 MSP |
| `0x08010004` | APP 复位处理函数 Reset_Handler |

### 8.2 初始栈指针检查

第一版至少检查 MSP 位于主 SRAM：

```text
0x20000000 ≤ MSP ≤ 0x20020000
```

需要允许 MSP 等于 SRAM 顶部 `0x20020000`。

如果未来 APP 把栈放入 CCM SRAM，再增加以下范围：

```text
0x10000000 ≤ MSP ≤ 0x10010000
```

### 8.3 Reset_Handler 检查

Reset_Handler 必须同时满足：

- 去掉 Thumb 标志位后的地址位于 APP 分区。
- 地址最低位为 1，表示 Cortex-M Thumb 状态。
- 不能为 `0xFFFFFFFF` 或 `0x00000000`。

### 8.4 返回明确的检查结果

建议 APP 检查函数返回枚举，而不是只返回真假：

```text
APP_VALID
APP_INVALID_MSP
APP_INVALID_RESET_HANDLER
APP_EMPTY
APP_INVALID_CRC
```

这样串口可以输出准确的失败原因。

### 8.5 验收标准

- 未烧录 APP 时，Bootloader 判断 APP 无效并停留。
- APP 首地址为擦除态时，Bootloader 不跳转。
- MSP 越界时，Bootloader 不跳转。
- Reset_Handler 越界或 Thumb 位错误时，Bootloader 不跳转。
- 合法 APP 能通过检查。

## 9. 阶段四：安全跳转 APP

### 9.1 跳转前清理顺序

建议采用以下顺序：

1. 等待 USART1 最后一个字节发送完成。
2. 关闭 Bootloader 使用的外设或其中断。
3. 关闭 SysTick。
4. 全局关闭中断。
5. 禁用所有 NVIC 外部中断。
6. 清除所有 NVIC 挂起标志。
7. 将 `SCB->VTOR` 设置为 `0x08010000`。
8. 执行数据同步和指令同步屏障。
9. 从 `0x08010000` 读取 APP MSP。
10. 从 `0x08010004` 读取 APP Reset_Handler。
11. 将处理器切回特权线程模式并使用 MSP。
12. 设置 APP MSP。
13. 立即跳转到 APP Reset_Handler。

### 9.2 关键约束

- 设置 MSP 后不能继续执行需要使用旧栈的普通 C 代码。
- 设置 MSP 应尽量靠近最终函数指针跳转。
- Bootloader 第一版继续使用轮询串口，不启用中断和 DMA。
- APP 必须自行重新初始化时钟、GPIO、USART、DMA、SysTick 和 FreeRTOS。
- APP 的 `SystemInit()` 仍必须设置正确的 VTOR，不能只依赖 Bootloader 设置。

### 9.3 期望日志

合法 APP：

```text
Bootloader start
APP valid
Jump to APP
APP start
```

非法 APP：

```text
Bootloader start
APP invalid: reset handler
Stay in bootloader
```

### 9.4 验收标准

- 复位后能从 Bootloader 进入天气时钟 APP。
- APP 的 FreeRTOS 调度正常。
- LVGL 刷新正常。
- SysTick、USART、DMA、RTC 等中断正常。
- 连续复位 20 次均能稳定进入 APP。
- APP 无效时始终停留在 Bootloader，不产生 HardFault 或复位循环。

## 10. 阶段五：Bootloader 驻留条件

### 10.1 推荐驻留条件

满足任意条件时停留在 Bootloader：

- APP 无效。
- 启动时按住升级按键。
- 启动后的短时间窗口内收到串口升级命令。
- APP 在复位前写入了升级请求标志。

### 10.2 第一版推荐策略

```text
初始化串口
  ↓
打印启动信息
  ↓
检查升级按键
  ↓
等待串口命令 500～1000 ms
  ↓
检查 APP
  ├─ 请求升级 → 驻留
  ├─ APP 无效 → 驻留
  └─ APP 有效 → 跳转
```

### 10.3 软件升级标志

第一版可以暂不实现持久化升级标志。后续可考虑：

- RTC 备份寄存器。
- 备份 SRAM。
- W25Q128 元数据区。

不建议频繁在 Bootloader 自身所在的内部 Flash 扇区中更新标志，因为 STM32F407 的最小擦除单位是整个扇区，处理不当可能破坏 Bootloader。

### 10.4 验收标准

- 按住指定按键复位后不跳转 APP。
- 收到升级命令后不跳转 APP。
- 不满足驻留条件且 APP 合法时自动进入 APP。
- APP 损坏时设备仍能进入串口升级模式。

## 11. 阶段六：USART1 IAP 升级协议

### 11.1 推荐帧结构

建议采用二进制分包协议：

```text
帧头
协议版本
命令
序号
目标偏移
数据长度
数据
CRC16
```

每个数据包建议为 256～1024 字节，禁止把完整固件一次性读入 SRAM。

### 11.2 基础命令

| 命令 | 作用 |
|---|---|
| `INFO` | 查询芯片、Bootloader 版本、分区和最大固件长度 |
| `BEGIN` | 发送固件长度、版本和期望 CRC32 |
| `ERASE` | 擦除 APP 扇区 |
| `DATA` | 写入一个数据块 |
| `END` | 结束传输并验证完整镜像 |
| `RUN` | 跳转 APP |
| `RESET` | 软件复位 |
| `ACK` | 命令成功 |
| `NACK` | 命令失败，并携带错误码 |

### 11.3 升级状态机

```text
IDLE
  ↓ BEGIN
READY
  ↓ ERASE
ERASED
  ↓ DATA
RECEIVING
  ↓ END
VERIFYING
  ├─ CRC 错误 → ERROR
  └─ CRC 正确 → VALID
                       ↓
                     RESET
```

任何乱序命令、重复序号、超长数据、地址越界或 CRC 错误都必须返回 NACK。

### 11.4 Flash 擦写流程

1. 检查固件长度不超过 `0xF0000`。
2. 解锁内部 Flash。
3. 清除 Flash 错误标志。
4. 只擦除 Sector 4～11 中固件实际覆盖的扇区。
5. 按 Flash 编程要求进行地址对齐。
6. 每次写入后立即回读比较。
7. 记录已接收字节数和下一包序号。
8. 接收完成后重新计算完整镜像 CRC32。
9. 校验成功后锁定 Flash。

### 11.5 向量表最后写入

为避免升级中途掉电后误跳转到残缺 APP，建议：

1. 擦除 APP 扇区。
2. 将固件最前面的向量表内容暂存在 SRAM。
3. 先写入 `0x08010008` 之后的数据。
4. 完整镜像 CRC32 校验成功后，再写入 MSP 和 Reset_Handler。
5. 最后回读向量表并复位。

这样升级未完成时，APP 前两个向量仍是 `0xFFFFFFFF`，Bootloader 会判断 APP 无效并继续等待升级。

### 11.6 上位机工具

建议后续编写一个独立 Python 工具，负责：

- 打开串口。
- 读取 BIN 文件。
- 计算文件长度和 CRC32。
- 发送升级命令和数据包。
- 超时重发。
- 显示升级进度。
- 保存升级日志。

上位机第一版使用 BIN 文件更简单；HEX 文件需要额外解析地址记录和空洞区间。

### 11.7 验收标准

- 正常传输能够完成 APP 更新。
- 数据包 CRC 错误时拒绝写入。
- 数据包序号错误时返回 NACK。
- 地址越界时拒绝写入。
- 升级中途断电后仍能重新进入 Bootloader。
- CRC32 不一致时不写入最终向量表。
- 升级成功后复位并进入新 APP。

## 12. 阶段七：W25Q128 OTA

串口 IAP 稳定后，再增加外部 Flash OTA：

```text
APP 通过网络下载候选固件
  ↓
分块写入 W25Q128 候选区
  ↓
校验长度、版本、CRC 或摘要
  ↓
设置 pending 升级状态
  ↓
软件复位
  ↓
Bootloader 读取候选固件
  ↓
擦除并重写内部 APP 分区
  ↓
完整校验
  ↓
写入最终向量并进入 APP
```

这一阶段还需要考虑：

- 候选固件区。
- 稳定回滚固件区。
- 元数据双副本。
- 升级状态机。
- 掉电恢复。
- 版本回退保护。
- 固件签名或至少 SHA-256 校验。

本阶段不应与最初的 APP 跳转功能同时开发。

## 13. 测试矩阵

| 编号 | 测试场景 | 预期结果 |
|---|---|---|
| T01 | 只烧录 Bootloader | 打印启动信息，判断 APP 无效并驻留 |
| T02 | Bootloader 和合法 APP | 打印启动信息后进入 APP |
| T03 | APP 首字为 `0xFFFFFFFF` | 不跳转，等待升级 |
| T04 | APP MSP 越界 | 不跳转，输出 MSP 错误 |
| T05 | APP Reset_Handler 越界 | 不跳转，输出入口错误 |
| T06 | Reset_Handler Thumb 位为 0 | 不跳转 |
| T07 | 单次按 RESET | Bootloader 只启动一次 |
| T08 | 连续复位 20 次 | 每次都稳定进入 APP |
| T09 | 按住升级按键复位 | 停留在 Bootloader |
| T10 | 串口收到升级命令 | 停留在 Bootloader |
| T11 | 数据包 CRC 错误 | 返回 NACK，不写入错误包 |
| T12 | 写入地址进入 Bootloader 区 | 拒绝命令，不擦写 Bootloader |
| T13 | 升级中途断电 | 下次启动仍停留在 Bootloader |
| T14 | 完整镜像 CRC 错误 | APP 保持无效，不写最终向量 |
| T15 | 升级成功 | 复位后进入新 APP |
| T16 | APP FreeRTOS 和 LVGL 运行 | 调度、显示和中断均正常 |

## 14. 每阶段验证记录

每个阶段完成后，应至少记录：

- 修改了哪些文件。
- Bootloader 构建结果。
- APP 构建结果。
- Bootloader HEX 地址范围。
- APP HEX 地址范围。
- 是否存在地址重叠。
- 是否实际执行下载。
- 使用的下载器和下载配置。
- 串口实际输出。
- APP 是否进入 `main()`。
- FreeRTOS、LVGL 和中断是否正常。
- 尚未验证的风险。

## 15. 推荐执行顺序

严格按照以下顺序实施：

1. 固定当前 Bootloader 构建基线。
2. 迁移 APP IROM 到 `0x08010000 / 0xF0000`。
3. 修改 APP `VECT_TAB_OFFSET` 为 `0x10000`。
4. 验证 APP HEX 地址和独立运行。
5. 增加 APP 向量合法性检查。
6. 增加安全跳转函数。
7. 验证 Bootloader 到 FreeRTOS/LVGL APP 的跳转。
8. 增加按键和串口驻留条件。
9. 增加 APP 分区擦除和写入。
10. 增加数据包 CRC16 和镜像 CRC32。
11. 增加向量表最后写入机制。
12. 编写串口升级上位机。
13. 完成掉电、错误包和越界测试。
14. 串口 IAP 稳定后，再设计 W25Q128 OTA。

## 16. 下一次开发范围

下一次建议只完成以下内容：

- APP 地址迁移。
- APP 向量表偏移修改。
- Bootloader APP 合法性检查。
- Bootloader 安全跳转。
- 两个 HEX 地址范围检查。

下一次暂不实现：

- 内部 Flash 擦写。
- 串口升级协议。
- CRC32 固件验证。
- W25Q128 OTA。

完成上述范围后，由用户分别烧录 Bootloader 和 APP，重点验证：

```text
Bootloader start
APP valid
Jump to APP
APP start
```

以及天气时钟的 FreeRTOS、LVGL、SysTick 和外设中断是否正常。
