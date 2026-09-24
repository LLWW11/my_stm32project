"""经 USART1 将内部地址为 0x08010000 的 APP BIN 暂存到 W25Q128。"""

import argparse
import binascii
import pathlib
import struct
import sys
import time


APP_START = 0x08010000
APP_END = 0x08100000
MAX_IMAGE_SIZE = 0xF0000
PACKET_SIZE = 256


def validate_image(path: pathlib.Path) -> tuple[bytes, int]:
    """读取并检查 BIN 的长度、初始 MSP、复位入口，返回数据及 CRC32。"""
    data = path.read_bytes()
    if not 8 <= len(data) <= MAX_IMAGE_SIZE:
        raise ValueError(f"BIN 长度 {len(data)} 不在 8～{MAX_IMAGE_SIZE} 字节范围内")

    msp, reset = struct.unpack_from("<II", data)
    if not (0x20000000 <= msp <= 0x20020000 and msp % 8 == 0):
        raise ValueError(f"初始 MSP 无效：0x{msp:08X}")
    if not (reset & 1 and APP_START + 8 <= (reset & ~1) < min(APP_START + len(data), APP_END)):
        raise ValueError(f"Reset_Handler 无效：0x{reset:08X}，请检查 APP 链接地址")

    return data, binascii.crc32(data) & 0xFFFFFFFF


def read_log_line(port) -> str:
    """读取并显示一行设备串口日志。"""
    line = port.readline().decode("utf-8", errors="replace").strip()
    if line:
        print(line, flush=True)
    return line


def wait_for_text(port, marker: str, timeout: float) -> None:
    """在限定时间内等待设备输出指定文本。"""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if marker in read_log_line(port):
            return
    raise TimeoutError(f"等待设备输出 {marker!r} 超时")


def wait_for_update_prompt(port, timeout: float) -> None:
    """等待正常启动或安装失败驻留时的升级提示，并报告串口无数据。"""
    deadline = time.monotonic() + timeout
    next_notice = time.monotonic() + 10.0
    received_output = False

    while time.monotonic() < deadline:
        line = read_log_line(port)
        if line:
            received_output = True
        if "[BOOT] Send U" in line:
            return
        if "[BOOT] Jump now" in line or "LVGL run before" in line:
            raise RuntimeError(
                "已错过 Bootloader 升级窗口，或板上仍是旧版 Bootloader；"
                "请确认启动日志含 W25 UART updater WUP1，再在脚本等待时复位"
            )
        if time.monotonic() >= next_notice:
            if not received_output:
                print(
                    "[PC] 尚未收到任何串口字节：请在脚本运行后按开发板 NRST，"
                    "并核对 COM 口、115200 波特率和 USART1 接线。",
                    flush=True,
                )
            next_notice += 10.0

    if not received_output:
        raise TimeoutError(
            "60 秒内 COM 口没有设备输出；确认开发板已复位、串口号正确，"
            "并用独立串口终端检查是否能看到 [BOOT] Bootloader start"
        )
    raise TimeoutError("收到设备输出，但没有升级入口；请核对板上是否烧录了新版 Bootloader")


def read_ack(port, timeout: float) -> None:
    """显示 ACK 之前的 Bootloader 日志，并等待单字节 K 确认。"""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = port.read(1)
        if result == b"K":
            return
        if result in (b"", b"\r", b"\n"):
            continue
        line = (result + port.readline()).decode("utf-8", errors="replace").strip()
        if line:
            print(line, flush=True)
        if "[BOOT]" in line and ("FAIL" in line or "invalid" in line or "timeout" in line):
            raise RuntimeError(f"设备拒绝数据：{line}")
    raise TimeoutError("等待设备 ACK 超时")


def send_image(port, data: bytes, crc: int) -> None:
    """发送开始帧和逐包 CRC32 校验的 BIN 字节。"""
    port.write(struct.pack("<4sII", b"WUP1", len(data), crc))
    read_ack(port, 180.0)  # 设备在 ACK 前擦除全部所需 W25 扇区。

    for offset in range(0, len(data), PACKET_SIZE):
        chunk = data[offset:offset + PACKET_SIZE]
        port.write(chunk + struct.pack("<I", binascii.crc32(chunk) & 0xFFFFFFFF))
        read_ack(port, 15.0)
        if (offset + len(chunk)) % (32 * 1024) == 0 or offset + len(chunk) == len(data):
            print(f"设备已确认 {offset + len(chunk)}/{len(data)} 字节", flush=True)


def monitor_result(port, marker: str, timeout: float) -> None:
    """监视暂存、安装与新 APP 的预期版本日志。"""
    deadline = time.monotonic() + timeout
    staged = False
    installed = False
    while time.monotonic() < deadline:
        line = read_log_line(port)
        if "[BOOT] STAGED OK" in line:
            staged = True
        if "[BOOT] Install PASS" in line:
            installed = True
        if "[BOOT]" in line and ("FAIL" in line or "invalid" in line):
            raise RuntimeError(f"设备报告失败：{line}")
        if marker in line:
            if staged and installed:
                print("升级验证 PASS：已看到新 APP 的版本标志", flush=True)
                return
            raise RuntimeError("看到 APP 标志，但缺少暂存或安装成功日志")
    raise TimeoutError("等待暂存、安装及 APP 版本日志超时；请检查串口完整日志")


def open_update_port(serial_type, port_name: str):
    """先停用 DTR/RTS，再打开升级串口，避免控制线影响板级复位。"""
    port = serial_type(port=None, baudrate=115200, timeout=1, write_timeout=10)
    # 与已在开发板上验证可用的 miniterm --dtr 0 --rts 0 保持一致。
    port.dtr = False
    port.rts = False
    port.port = port_name
    port.open()
    return port


def main() -> int:
    """解析命令行、等待 Bootloader 入口并发送一个 APP BIN。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="调试串口，例如 COM5")
    parser.add_argument("bin_file", type=pathlib.Path, help="链接在 0x08010000 的 APP BIN")
    parser.add_argument("--expect", default="OTA test version: v2", help="新 APP 的预期日志片段")
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("缺少 pyserial：先运行 python -m pip install pyserial", file=sys.stderr)
        return 2

    try:
        data, crc = validate_image(args.bin_file)
        print(f"BIN: {len(data)} 字节，CRC32=0x{crc:08X}")
        with open_update_port(serial.Serial, args.port) as port:
            print("请现在复位开发板；脚本将于启动窗口发送 U。", flush=True)
            wait_for_update_prompt(port, 60.0)
            port.write(b"U")
            wait_for_text(port, "[BOOT] RX READY", 15.0)
            send_image(port, data, crc)
            monitor_result(port, args.expect, 180.0)
        return 0
    except (OSError, ValueError, TimeoutError, RuntimeError) as error:
        print(f"升级未完成：{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
