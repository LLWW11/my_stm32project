# 优先使用 Yocto SDK 导出的 CC，保留其中的 CPU、浮点 ABI 和 sysroot 参数。
# 仅在未指定 CC 时替换 GNU Make 的内置 cc；同时兼容原有 CROSS_COMPILE 用法。
ifneq ($(filter default undefined,$(origin CC)),)
CC = $(CROSS_COMPILE)gcc
endif

# 保留 SDK 的编译与链接变量，工程必需的 C99 和警告参数单独追加。
CFLAGS ?= -O2
LDLIBS ?=
IAP_CFLAGS = -std=c99 -Wall -Wextra -Werror

# 主机测试使用本机 gcc，避免运行 SDK 交叉编译生成的 ARM 程序。
HOSTCC ?= gcc
HOSTCFLAGS ?= -O2
HOSTCPPFLAGS ?=
HOSTLDFLAGS ?=
HOSTLDLIBS ?=

all: can_iap_send

# CC 是完整命令，不能给整个变量加引号，否则编译器和参数会被当成一个文件名。
can_iap_send: main.c can_iap_sender.c socketcan_transport.c can_iap_sender.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IAP_CFLAGS) main.c can_iap_sender.c socketcan_transport.c $(LDFLAGS) $(LDLIBS) -o $@

# 直接包含实际 APP 接收函数和 Bootloader CRC，头文件变更后同样重新构建。
TEST_DEPS = ../../app/CAN_IAP_Task.c ../../app/dbg_config.h \
            ../../bootloader/app/boot_crc.c ../../bootloader/app/boot_crc.h \
            ../../bootloader/app/boot_Image_desc.h ../../bootloader/app/boot_update.h \
            ../../my_driver/25q128/25q128.h tests/stubs/can_iap.h tests/stubs/task.h

test_can_iap_sender: tests/test_can_iap_sender.c tests/boot_crc_host.c can_iap_sender.c can_iap_sender.h $(TEST_DEPS)
	$(HOSTCC) $(HOSTCPPFLAGS) $(HOSTCFLAGS) $(IAP_CFLAGS) -UNDEBUG -I. -Itests/stubs -I../../app -I../../my_driver -I../../bootloader/app can_iap_sender.c tests/boot_crc_host.c tests/test_can_iap_sender.c $(HOSTLDFLAGS) $(HOSTLDLIBS) -o $@

test: test_can_iap_sender
	./test_can_iap_sender

# 离线预检目标仅供开发机运行，不能执行真实 CAN 传输。
can_iap_dry_run: main.c can_iap_sender.c tests/offline_transport.c can_iap_sender.h
	$(HOSTCC) $(HOSTCPPFLAGS) $(HOSTCFLAGS) $(IAP_CFLAGS) -I. main.c can_iap_sender.c tests/offline_transport.c $(HOSTLDFLAGS) $(HOSTLDLIBS) -o $@

# 打印当前实际使用的工具链，便于确认 SDK 环境已生效。
print-config:
	@echo "CC=$(CC)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "CFLAGS=$(CFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "LDLIBS=$(LDLIBS)"
	@echo "HOSTCC=$(HOSTCC)"

# 只清理本工具的生成文件，保留用于传输的 BIN 镜像和源码。
clean:
	$(RM) can_iap_send can_iap_send.exe can_iap_dry_run can_iap_dry_run.exe test_can_iap_sender test_can_iap_sender.exe main.o

.PHONY: all test clean print-config
