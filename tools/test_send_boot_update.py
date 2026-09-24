"""验证电脑端升级工具的向量检查和分包格式。"""

import binascii
import contextlib
import io
import struct
import unittest

import send_boot_update as update


class FakePath:
    """为校验测试提供内存中的 BIN。"""

    def __init__(self, data: bytes):
        """保存待验证的 BIN 字节。"""
        self.data = data

    def read_bytes(self) -> bytes:
        """返回模拟文件的全部内容。"""
        return self.data


class FakePort:
    """记录发送帧并为每包返回确认字节。"""

    def __init__(self):
        """建立空的发送帧记录。"""
        self.writes = []

    def write(self, data: bytes) -> int:
        """记录串口发送内容。"""
        self.writes.append(data)
        return len(data)

    def read(self, length: int) -> bytes:
        """模拟设备的单字节 ACK。"""
        return b"K" if length == 1 else b""


class LoggingPort:
    """模拟 ACK 之前先输出接收进度的 Bootloader。"""

    def __init__(self, message: bytes):
        """保存待输出的日志并准备一个 ACK。"""
        self.message = message
        self.calls = 0

    def read(self, length: int) -> bytes:
        """先给出日志首字节，之后给出 ACK。"""
        self.calls += 1
        return b"[" if self.calls == 1 else b"K"

    def readline(self) -> bytes:
        """返回日志的剩余字节。"""
        return self.message


class LinePort:
    """按顺序提供 Bootloader 启动日志。"""

    def __init__(self, lines: list[bytes]):
        """保存要模拟的串口日志行。"""
        self.lines = list(lines)

    def readline(self) -> bytes:
        """返回下一行串口日志。"""
        return self.lines.pop(0) if self.lines else b""


class ClosedSerialPort:
    """模拟 pySerial 默认激活控制线但尚未打开的串口。"""

    def __init__(self, port, baudrate, timeout, write_timeout):
        """记录打开端口时需要核验的配置。"""
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.write_timeout = write_timeout
        self.dtr = True
        self.rts = True
        self.opened = False

    def open(self):
        """确认控制线在端口打开之前均已停用。"""
        if self.port != "COM6" or self.dtr or self.rts:
            raise AssertionError("串口打开前未正确设置端口或控制线")
        self.opened = True


class BootUpdateSenderTests(unittest.TestCase):
    """检查升级文件约束与设备协议一致。"""

    def test_control_lines_inactive_before_open(self):
        """模拟默认激活的 DTR/RTS，确认两线在打开 COM6 前停用。"""
        port = update.open_update_port(ClosedSerialPort, "COM6")
        self.assertTrue(port.opened)
        self.assertEqual(port.baudrate, 115200)
        self.assertEqual(port.timeout, 1)
        self.assertEqual(port.write_timeout, 10)

    def test_vector_and_crc(self):
        """校验已重定位 BIN 的向量及标准 CRC32。"""
        data = struct.pack("<II", 0x20016748, update.APP_START + 9) + b"12345678"
        image, crc = update.validate_image(FakePath(data))
        self.assertEqual(image, data)
        self.assertEqual(crc, binascii.crc32(data))

        wrong_address = struct.pack("<II", 0x20016748, 0x0800025D) + b"12345678"
        with self.assertRaises(ValueError):
            update.validate_image(FakePath(wrong_address))

    def test_packet_layout(self):
        """验证开始帧、256 字节切包及每包小端 CRC32。"""
        image = bytes(range(256)) * 2 + b"Z"
        port = FakePort()
        update.send_image(port, image, binascii.crc32(image))

        self.assertEqual(port.writes[0], struct.pack("<4sII", b"WUP1", 513, binascii.crc32(image)))
        self.assertEqual(len(port.writes), 4)
        for index, frame in enumerate(port.writes[1:]):
            chunk = image[index * 256:(index + 1) * 256]
            self.assertEqual(frame, chunk + struct.pack("<I", binascii.crc32(chunk)))

    def test_ack_keeps_bootloader_log_visible(self):
        """确认进度日志会显示，并且不会被误判为 ACK 错误。"""
        port = LoggingPort(b"BOOT] W25 verified bytes = 0x00008000\r\n")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            update.read_ack(port, 1.0)
        self.assertIn("W25 verified bytes", output.getvalue())

    def test_recovery_prompt_is_accepted(self):
        """安装失败后的驻留提示也应触发发送流程。"""
        port = LinePort([b"[BOOT] Send U to stage a new BIN\r\n"])
        with contextlib.redirect_stdout(io.StringIO()):
            update.wait_for_update_prompt(port, 1.0)

    def test_old_bootloader_exits_early(self):
        """旧 Bootloader 已跳转 APP 时应立即给出原因。"""
        port = LinePort([b"[BOOT] Bootloader start\r\n", b"[BOOT] Jump now\r\n"])
        with contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, "升级窗口"):
                update.wait_for_update_prompt(port, 1.0)


if __name__ == "__main__":
    unittest.main()
