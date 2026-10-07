# i.MX6ULL 镜像传输工具

这是匹配当前 WeatherClock APP 的 C / SocketCAN 命令行程序，不依赖第三方 CAN 库。它主动发送原始 APP BIN，STM32 在接收任务中写 W25Q128、校验 CRC、提交 READY，再复位交给 Bootloader 安装。

当前镜像头仍为 **24 字节，无版本号**。发送端已适配 STM32 的 AT24C02 续传记录：重新运行同一 BIN 时，START 返回已保存的正文偏移，发送端从该位置继续。2026-10-06 用户反馈，修复 I2C1 初始化后，传输过程中按 STM32 RESET 再续传已正常。当前没有定时查询和版本比较。

## 使用当前 Yocto SDK 编译

当前使用 GNU Make，不需要 CMake 或 Ninja。正式程序生成在工具目录中，文件名为 `can_iap_send`。

你提供的 `$CC` 是完整命令：

```sh
arm-poky-linux-gnueabi-gcc -march=armv7ve -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi
```

Makefile 保留完整的 `CC`，同时使用 SDK 的 `CPPFLAGS`、`CFLAGS`、`LDFLAGS` 和 `LDLIBS`，符合 [Yocto SDK 的环境与构建方式](https://docs.yoctoproject.org/2.1/sdk-manual/sdk-manual.html#makefile-based-projects)。其中 `-mfloat-abi=hard` 已明确指定硬浮点 ABI，不能仅凭编译器名称中的 `gnueabi` 删除这些参数或换成另一套工具链。

你当前终端已经能输出正确的 `$CC`，在 Ubuntu 开发机上执行：

```sh
cd ~/linux/IMX6ULL/linux_app/999imx6ull_can_iap

echo "$CC"
make print-config CC="$CC"
make clean
make CC="$CC"
file can_iap_send
```

`make CC="$CC"` 显式把当前终端的完整命令传给 Make，变量即使没有导出到子进程也能正确使用。若 SDK 已导出 `CC`，直接 `make` 同样会沿用 SDK；Makefile 只有在未指定编译器时才退回本机 gcc。

新开的终端需要先加载 SDK 提供的实际 `environment-setup-*` 脚本，再执行以上命令。不要只设置一个裸的 `arm-poky-linux-gnueabi-gcc`，否则可能丢失 ABI 与 sysroot 参数。更换编译器或参数后先运行 `make clean`，避免复用原有程序。

如果老旧 SDK 链接时提示找不到 `clock_gettime`：

```sh
make clean
make CC="$CC" LDLIBS=-lrt
```

原生 Linux 构建仍可使用 `make CC=gcc`；原来的前缀方式也保留为 `make CROSS_COMPILE=实际前缀`，但在当前 SDK 环境中优先使用完整的 `$CC`。项目默认添加 C99、`-Wall -Wextra -Werror`，SDK 的优化与链接参数会继续生效。

## 主机测试与离线预检

协议测试直接编译真实 APP 接收函数及 Bootloader CRC，需保留完整 WeatherClock 项目目录。只单独复制发送工具目录到 Ubuntu 时，编译正式程序不需要父目录中的 STM32 源码。

主机目标单独使用 `HOSTCC`，不使用 SDK 的 ARM 编译器及目标参数，避免在 Ubuntu 上运行 ARM 测试程序：

```sh
make test HOSTCC=gcc
make can_iap_dry_run HOSTCC=gcc
./can_iap_dry_run --file ./weatherClock.bin --dry-run
```

如果指定主机优化选项，可使用 `HOSTCFLAGS`；测试保留断言，即使包含 `-DNDEBUG` 也不会关闭验收检查。`can_iap_dry_run` 只做离线预检，不能执行真实 CAN 传输。正式交叉编译出的 `can_iap_send` 应在 i.MX6ULL 上运行，其 `--dry-run` 同样只检查 BIN。

## 文件与构建目标

| 文件或目标 | 用途 |
|---|---|
| `Makefile` | SDK 交叉编译、主机测试与清理 |
| `main.c` | 命令行、读取 BIN 与传输入口 |
| `can_iap_sender.c/.h` | 分片、CRC、ACK 与超时重传 |
| `socketcan_transport.c` | Linux SocketCAN 收发 |
| `tests/` | 主机协议测试、真实 CRC 入口和离线传输替代实现 |
| `weatherClock.bin` | 当前联调镜像输入，清理目标会保留它 |
| `make` | 编译正式程序 `can_iap_send` |
| `make test` | 用主机编译器编译并运行协议测试 |
| `make can_iap_dry_run` | 编译主机离线预检程序 |
| `make print-config` | 显示实际编译器及参数 |
| `make clean` | 只删除本工具的生成程序和中间文件 |

## 板端操作

1. 先用调试器烧入本次修改后的正常 Bootloader 和 APP。发送工具要求新的回复标签，旧 APP 会提示 `Untagged ACK`。
2. 将 `can_iap_send` 与待升级 `weatherClock.bin` 复制到 i.MX6ULL。BIN 必须由链接到 `0x08010000` 的 APP 生成，不能发送 HEX、AXF、Bootloader BIN 或手工拼接的 W25 镜像头。
3. 先做只读预检，之后配置 CAN 接口并发送。

```sh
chmod +x ./can_iap_send
./can_iap_send --file ./weatherClock.bin --dry-run

# 需要接口管理权限；若 can0 已按相同参数启用，可跳过重新配置。
ip link set can0 down
ip link set can0 type can bitrate 500000 restart-ms 100
ip link set can0 up
ip -details -statistics link show can0

./can_iap_send --interface can0 --file ./weatherClock.bin
```

CAN 连接沿用已验证的双节点接线、收发器与终端配置。STM32 的位时序要求 APB1 为 42 MHz：`42 MHz / (6 × (1 + 11 + 2)) = 500 kbit/s`。如果改动系统时钟，应重新核对。

同一时间只运行一个升级发送进程。STM32 的 `ENABLE_CAN_TEST` 应为 0，串口先看到 `[IAP] task ready, W25 OK` 再发送。正常 Bootloader 的 HEX 是 `bootloader/mdk/build/f407_boot/weatherClock_bootloader.hex`，APP 的 HEX 是 `mdk/build/f407/weatherClock.hex`；HEX 已含绝对地址。不要使用 `BOOT_W25_WRITER_MODE` 临时直跳 Bootloader 来验证安装。

## 中断后的续传操作

使用适配检查点续传的发送程序，并确保 STM32 已烧入包含会话绑定修正和 I2C1 初始化修复的 APP。正常 Bootloader 安装协议不变。无需新增 `--resume` 参数，也无需在 i.MX6ULL 上保存偏移文件。

1. 传输中停止发送进程或恢复 CAN 连接后，等待 STM32 距最后一帧超过三秒并回到 IDLE；串口可看到 `[IAP] CAN RX timeout`。若 STM32 重启，则等待 IAP 任务初始化完成。
2. 使用原来的同一个 BIN 再运行发送命令。每次运行可使用不同会话号，首个合法 DATA 会重新绑定会话。

```sh
sleep 4
./can_iap_send --interface can0 --file ./weatherClock.bin
```

STM32 串口应出现 `[IAP] AT24C02 OK, resume enabled`。有有效断点时，串口出现 `[IAP] RESUME from ...`，发送端出现 `RESUME offset=.../...`。START 长度与 CRC32 必须和记录一致，记录 CRC、状态和偏移范围必须有效，且 W25 头部未处于 READY/DONE；否则接收端擦除并返回偏移 0，发送端显示 `NEW transfer offset=0/...`。

EEPROM 每接收约 4 KiB 或正文收齐时保存进度。普通 DATA ACK 表示当前 W25 写入进度，不表示每帧都保存了 EEPROM 检查点；重启后可能从更早位置重发。发送端按 START 返回的偏移恢复，例如接收到了 7000 字节但只保存到 4096 字节时，从 4096 继续。若第一份检查点尚未保存、记录损坏、EEPROM 不可用或 BIN 已变化，会从零开始。

若断点已等于 BIN 长度，发送端先重发最后一个字节的 DATA，接收端回读核对尾部并绑定新会话，不重复写正文；随后发送 END，重新执行完整 CRC 和 READY 提交。

目前采用手动重新运行恢复：程序会对短暂 DATA 丢包重试，重试耗尽或链路故障时退出，不在后台自动重连。若 END 已发送但未收到确认，仍返回退出码 2；先看串口确认是否已 READY/安装完成，再决定是否重新发送。READY/DONE 镜像的记录不会用于续传，盲目重跑可能触发全量重新擦除。

## 已修复的 EEPROM 初始化问题

2026-10-06 联调时，STM32 曾输出 `[IAP] AT24C02 missing, resume disabled`。此时 CAN 传输仍可写入 W25Q128，但不保存 AT24C02 检查点；RESET 清除 RAM 中的接收上下文，重新 START 会擦除镜像区并从零传输。

用户定位到 `my_driver/board/IIC.c` 中 `IIC1_Init()` 末尾的 `(void)IIC1_BusRecover();` 调用，删除后反馈续传恢复正常，并已提交 Git。当前源码保留 `IIC1_BusRecover()` 函数，但初始化不再调用；该文件最近一次提交为 `00a6c88`。

此前调用顺序为：先 `I2C_Init()` 配置时序并使能，再进入总线恢复；恢复函数内部执行 I2C 软件复位，末尾只重新使能，没有重新配置时序。这一顺序是源码中发现的可疑点，用户已通过删除调用验证它与本次故障相关；尚未提供复位前后寄存器实测，不能把某个寄存器被清零写成已测结论。

若后续版本重新出现 `missing, resume disabled`，先确认没有重新引入上述初始化调用，再检查 PB8/SCL、PB9/SDA、七位地址 `0x50`、器件供电和总线应答。`missing` 是探测失败的统一提示，不能单凭它判断芯片不存在。不要直接把续传开关强制设为真；只有 EEPROM 能正常保存并读回记录，复位续传才有断点可用。

## 协议与超时

全部为经典 CAN 的 11 位标准数据帧，整数小端。

| ID | DLC | 数据布局 |
|---|---:|---|
| `0x120` START | 8 | `[0..3] BIN 长度；[4..7] BIN CRC32` |
| `0x121` DATA | 5～8 | `[0] 会话；[1..3] 正文偏移；[4..] 1～4 字节正文` |
| `0x122` END | 5 | `[0] 会话；[1..4] BIN 长度` |
| `0x123` REPLY | 8 | `[0] ACK=0/NACK=1；[1] 会话；[2..4] 下一期望偏移；[5] 错误码；[6] 请求 ID 低字节；[7] 0xA5` |

回复的 `[6]` 分别为 START=`0x20`、DATA=`0x21`、END=`0x22`；接收任务主动报告间隔超时为 `0x00`。START 回复会话为 0，首个合法 DATA 绑定传输会话，DATA 和 END 回复必须匹配该会话。CRC 使用 CRC-32/ISO-HDLC，多项式 `0xEDB88320`，初值和最终异或均为 `0xFFFFFFFF`，标准测试串 `123456789` 的 CRC 为 `0xCBF43926`。

START 默认超时 180 秒：有有效续传记录时返回保存偏移；否则等待 W25 头部及所有正文扇区擦除完成后返回 0。主机只接受会话为 0、请求标签为 `0x20`、偏移不超过 BIN 长度的 START ACK；此期间不发送 DATA，不自动重发 START。DATA 每帧等待下一偏移的确认，默认 500 ms 超时、最多重传 3 次；重复分片由 APP 返回已落盘进度。END 默认等待 30 秒，允许全镜像读回 CRC 与头部提交，**不自动重发 END**。

```sh
./can_iap_send --interface can0 --file ./weatherClock.bin --session 73 \
  --start-timeout-ms 180000 --data-timeout-ms 500 \
  --end-timeout-ms 30000 --retries 3
```

错误码为：0 成功、1 帧或状态错误、2 长度或向量错误、3 偏移错误、4 会话错误、5 Flash 错误、6 CRC 错误、7 传输间隔超时。只有 DATA 的偏移 NACK 且期望偏移仍等于当前分片起点时才重传；其它 NACK 停止传输。DATA 超时最多允许配置为 1000 ms，以便在接收端 3000 ms 间隔超时前重试。

| 退出码 | 含义与下一步 |
|---:|---|
| 0 | `--dry-run` 检查通过，或收到明确的 END ACK，镜像已经 READY；安装成功仍须看串口 |
| 1 | 参数、文件、链路、协议失败或接收端 NACK；检查诊断和串口后再决定是否重传 |
| 2 | END 已发送但结果未确认；镜像可能已经 READY 或正在安装，先查看串口再决定下一步 |

传输采用四字节分片和逐帧落盘，约 900 KiB 镜像需要约 22.5 万个 DATA 帧及同等数量 ACK，耗时还受 W25 编程等待和任务调度影响。当前实现优先验证正确性，传输速度需上板测量。后续若改为页缓存和窗口确认，需要同时修改两端协议。

## 验收与恢复

成功接收后，串口应依次显示 APP 的 `CRC PASS`、`image READY`，再显示 Bootloader 的 `External CRC PASS`、`Internal CRC PASS`、`Install PASS`，最后进入新 APP。发送端的 `Image READY` 不能代替这些安装日志。

当前只有一份外部候选镜像，尚无回滚和安装后健康确认。传输阶段已有 AT24C02 检查点续传，会话号仍保存在 RAM。续传要求 APP 能正常运行并接收 START；EEPROM 是单份原地覆盖的带 CRC 记录，更新时掉电可能使记录失效，不能保证每次掉电都保留断点。若内部 APP 安装中断且 Bootloader 驻留，当前 CAN 接收在 APP 中，需通过调试器或外部 W25 写入方式恢复，不能依赖本工具向驻留 Bootloader 发送固件。

2026-10-06 本次通过 Windows GCC 的 `-std=c99 -Wall -Wextra -Werror` 主机测试与离线预检。测试直接调用真实 APP 接收函数和 Bootloader CRC，覆盖 18 个正常/故障场景、960 KiB 最大镜像、传输间隔中断与模拟接收端重启、全长断点换会话、8 种记录边界、EEPROM 写入失败和尾部损坏。模拟恢复后核对正文、CRC、READY，并确认没有重复擦除；三秒任务超时在测试中通过设置 IDLE 模拟，没有执行真实 FreeRTOS 任务循环。

2026-10-06 用户提供 i.MX6ULL 实际运行日志：BIN 长度 901404 字节、CRC32 `0xC0F6D482`，CAN 传输进度至少到 131072 字节；随后反馈删除初始化中的 `IIC1_BusRecover()` 调用后，STM32 RESET 中断后的续传已正常。本记录依据用户板端反馈，助手本轮没有重新编译、烧录或重复硬件测试。

目前已确认用户反馈的 RESET 续传场景。尚未收到 EEPROM 正在更新时实际断电、正文收齐但 END 未提交时硬件复位、EEPROM 不可用降级，以及本次镜像完整安装的详细日志；这些场景不写成已上板通过。发送端 READY 后仍须观察 Bootloader 安装和新 APP 日志。

2026-10-04 根据当前 SDK 环境恢复为 Makefile，并移除 CMake 配置与交叉工具链目录。正式构建沿用完整的 `$CC`；主机测试与离线程序通过独立的 `HOSTCC` 构建。后续源码或工具链参数变更后，仍应在 Ubuntu SDK 中重新编译，并用新的板端程序验证。

SocketCAN 套接字、标准帧过滤及错误帧处理依据 [Linux 内核 SocketCAN 文档](https://docs.kernel.org/networking/can.html)。
