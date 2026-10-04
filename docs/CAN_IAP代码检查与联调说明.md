# CAN 升级代码检查与联调说明

检查日期：2026-10-03。检查对象是当前工作区中的 APP、CAN1 驱动、共享镜像头、正常 Bootloader、EIDE 构建配置和独立 W25 烧写工程。

## 当前链路

`main_init → 创建 can_iap_task → CAN1 RX0 中断 → 16 帧队列 → START 擦除 → DATA 落盘 → END 读回 CRC/向量校验 → 写头部并提交 READY → 系统复位 → Bootloader 安装 → DONE → 复位进入 APP`。

CAN 初始化采用标准外设库；NVIC 分组为 4，RX0 抢占优先级为 7。FreeRTOS 允许调用中断 API 的优先级界限为 5，因此优先级 7 调用 `xQueueSendFromISR` 符合当前配置。中断只搬运帧，Flash 擦写与 CRC 在任务上下文进行。

IAP 空闲时用 `xQueueReceive(..., portMAX_DELAY)` 阻塞，到帧后由中断投递队列唤醒；接收中使用 3000 ms 队列等待。**镜像主动下发无需定期唤醒**。定时检查“是否有新版本”是另一项业务，目前没有处理 `0x124/0x125/0x126`，正式滤波器也不接收这些 ID。

## 已修正的问题

| 问题 | 原因及本次处理 |
|---|---|
| Bootloader 镜像头重复定义 | `boot_update.h` 和 `boot_Image_desc.h` 重复定义 `boot_image_header_t`；统一引用共享头 |
| FreeRTOS 堆不足 | 启用 LVGL 时，创建主循环前的任务栈为 1024+1024+2048+4096 个 32 位字，即 32 KiB，尚未计入空闲/定时器任务及 TCB、队列；原堆仅 30 KiB。本次改为 48 KiB，并检查相关任务创建结果 |
| DATA 与 END 的回执无法区分 | 旧回复没有阶段字段，迟到 DATA ACK 可能假冒 END 确认；使用原保留字节标记请求 ID 和 `0xA5`，发送端校验阶段 |
| 发送 NULL 参数检查失效 | 原条件 `len > 8 && data == NULL` 无法保护合法非零长度；改为 `len > 0 && data == NULL` |
| CAN 初始化失败残留悬空队列指针 | 删除队列后置 NULL，ISR 增加空指针保护 |
| 双节点测试收不到 `0x321` | 测试任务显式切换为测试 ID 滤波器；正式 IAP 接收 START/DATA/END |
| 错误首帧抢占会话 | 移到首个合法顺序 DATA 才绑定会话 |
| 重复 START 再次擦除 | 传输接收态拒绝再次 START；发送端也不自动重发 START |
| CRC 正确但向量错误仍提交 READY | APP 增加与 Bootloader 相同的 MSP、Thumb 和复位入口范围检查 |
| 未验收元数据就提交 READY | 写入擦除态头部后先完整读回比较，成功才单独写 READY；提交后复核地址、长度、正文 CRC 等字段 |
| W25 标识检查失败仍进入任务循环 | 检查失败后退出 IAP 任务 |
| START 回执把长度低字节当会话 | START 回复固定会话 0；DATA/END 回显请求会话 |

新发送工具必须搭配本次带回复标签的 APP。Bootloader 不参与 CAN 协议，标签修改不改变它读取的 24 字节镜像头。未添加固件版本字段，也未改变查询周期。

## 镜像格式核对

| 偏移 | 当前 APP 与正常 Bootloader 的字段 |
|---:|---|
| 0 | magic=`0x31505557` |
| 4 | target=`0x08010000` |
| 8 | BIN length，范围 8～`0xF0000` |
| 12 | BIN 全量 CRC32 |
| 16 | header_crc，覆盖前 16 字节 |
| 20 | state：擦除态 `0xFFFFFFFF` → READY=`0xFFFFFFFE` → DONE=`0xFFFFFFFC` |

头部位于 W25 `0x000000`，正文从 `0x001000` 开始。头部结构及大小断言共用 `boot_Image_desc.h`，两端均以 `offsetof` 计算 CRC 范围和 state 地址。READY/DONE 不参与头部 CRC，状态只清位，符合 NOR 编程约束。BIN 不是“带头的升级包”，元数据由 APP 接收完成后生成。

**独立工程 `w25q128_writer_v1_0` 使用 28 字节、带版本的头部，与当前 24 字节源码不兼容。** 本次没有修改它。不能用它写出的 READY 头来验收当前正常 Bootloader；若重新启用该工程，必须统一三方字段布局、版本编码、CRC 范围及 state 偏移，再重新构建。之前的安装说明混用了两种格式，本次已注明区别。

Bootloader 在擦内部 APP 前检查外部完整 CRC 和向量，保留内部扇区 0～3，仅擦写 APP 扇区 4～11；正文先写、前八字节向量最后写，随后验证内部全量 CRC，再更新 DONE 并复位。`BOOT_W25_WRITER_MODE` 跳过安装，不适合本轮联调。

## 验证边界及待验证项

已经完成实际 APP 处理函数与发送协议核心的主机测试：正常传输、DATA 丢失、ACK 丢失、重复帧、错会话、乱序偏移、迟到 DATA ACK、CRC 错误、Flash 错误、提交前头部损坏、START 不重发、END 未确认等 16 个场景，以及 960 KiB 最大镜像和最后一帧非四字节长度检查。

APP 和正常 Bootloader 使用 ARMCC5 完整重编译通过；修改堆后重新编译相关依赖并链接通过。APP 最终静态 SRAM 占用 110416 字节，位于 128 KiB 主 SRAM 内，链接容量尚余 20656 字节；这不是 FreeRTOS 堆剩余量，也不是任务运行期栈余量。原有业务和 LVGL 编译警告仍存在，本次新增 CAN/IAP 文件未产生编译警告。

待板端验收：

1. 普通复位能完成 UI、Wi-Fi、主循环及 IAP 启动，检查实际剩余堆和任务栈余量。
2. 先跑 BIN 预检，再传输一个可恢复的小 APP；同时观察串口与 `candump`。
3. 查看 APP READY、Bootloader 外部/内部 CRC、安装成功、新 APP 启动的完整链路日志。
4. 再传实际大镜像并测量耗时、CAN 错误计数、接收队列溢出及 UI/Wi-Fi 并行表现。
5. 掉电测试单独设计：接收阶段可重新从零传输；安装阶段的恢复和新 APP 健康确认尚未完成验证。

仍需留意 W25 忙位紧密轮询：IAP 优先级 7，在擦除阶段可能较长时间阻挡优先级 5 的主循环。当前接收任务的间隔超时以队列等待为依据，任务长期未调度会影响墙钟意义上的超时。发送端逐帧停等限制队列压力，但业务实时性、吞吐和长期稳定性仍需上板测量。

本次未烧录、未实际擦写 Flash、未实测 CAN 传输。Linux SocketCAN 后端已按 [Linux 内核文档](https://docs.kernel.org/networking/can.html) 编写，仍需用 i.MX6ULL 的实际 Linux 交叉工具链编译及板端运行验证。具体命令见 `tools/imx6ull_can_iap/README.md`。
