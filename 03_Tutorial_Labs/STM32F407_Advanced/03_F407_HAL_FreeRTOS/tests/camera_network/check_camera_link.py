"""真实Python相机状态机产生线路数据，再由真实F407任务代码消费。

仅串口、时间、FreeRTOS队列使用替身；不把电脑测试写成实机或实时性能证据。
"""
import argparse
import copy
import struct
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--camera", type=Path, required=True)
    args = parser.parse_args()
    sys.path[:0] = [str(args.camera.resolve()), str(args.camera.resolve() / "tests")]
    from gateway import protocol as p
    from gateway.config import DEFAULTS
    from gateway.core import Gateway
    from gateway.network_worker import WifiScanner
    from test_scan import ScanBackend
    from test_gateway import Clock, FakeNetwork

    output = ROOT / "build/Camera_UART_Check"
    executable = output / "camera_link_test.exe"
    subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "tests/camera_network/stubs"),
                    "-I" + str(ROOT / "User/Hardware"), "-I" + str(ROOT / "User/Task"),
                    str(ROOT / "tests/camera_network/camera_link_test.c"), "-o", str(executable)], check=True)
    for variant in ("complete", "missing"):
        clock, sent, backend = Clock(), [], FakeNetwork()
        gateway = Gateway(99, backend, sent.append, clock, copy.deepcopy(DEFAULTS))
        exchanges = []

        def exchange(request):
            sent.clear()
            gateway.receive(request)
            exchanges.append([request.encode(), b"".join(sent)])

        with patch("gateway.core.os.urandom", return_value=struct.pack("<I", 0x12345678)):
            exchange(p.Frame(p.HELLO, 1, 0x30, struct.pack("<I", 0)))
        # 噪声、完整最大帧与坏CRC帧都不能阻塞后续有效握手。
        corrupt = bytearray(p.Frame(p.PING, 7, 99, b"bad").encode())
        corrupt[-1] ^= 1
        exchanges[0][1] = (b"noise\xaa" + p.Frame(p.UPLOAD, 8, 99, bytes(512)).encode()
                           + bytes(corrupt) + exchanges[0][1])
        exchange(p.Frame(p.HELLO, 2, 0x12345678, struct.pack("<I", 99)))
        # 序号3由F407的首次双向PING使用，扫描随后使用4。
        exchange(p.Frame(p.WIFI_SCAN, 4, 0x12345678, p.json_bytes({"target_session": 99})))
        scanner_backend = ScanBackend()
        scanner_backend.items = [{"ssid": "AP" + str(i), "rssi": -90 + i, "secured": True}
                                 for i in range(20)]
        scanner_backend.items.append({"ssid": "AP19", "rssi": -10, "secured": True})
        scanner_backend.items.append({"ssid": '网络"\\', "rssi": -12, "secured": False})
        scan = WifiScanner(scanner_backend, gateway.network_event, clock)
        scan.start(backend.commands[-1])
        clock.now = 0.4
        scan.tick()
        gateway.network_event({"type": "state", "state": {
            "wifi_intent": "on", "wifi": "connected", "ssid": "AP19",
            "ip": "192.0.2.8", "mqtt": "disabled", "error": ""}})
        gateway.tick()
        frames = p.Parser().feed(b"".join(sent), 0)
        if variant == "missing":
            frames = [x for x in frames if not (x.kind == p.SCAN_ITEM and x.payload[4] == 3)]
        exchanges[-1][1] = b"".join(x.encode() for x in frames)
        exchanges.append([p.Frame(p.PONG, 1, 0x12345678, struct.pack("<I", 99)).encode(), b""])
        trace = output / ("camera_link_" + variant + ".bin")
        with trace.open("wb") as stream:
            for request, response in exchanges:
                stream.write(struct.pack("<HH", len(request), len(response)))
                stream.write(request + response)
        subprocess.run([str(executable), str(trace), variant], check=True)
        # 同一线路再以128B大块取数，覆盖分段复制的满缓冲/粘包路径。
        subprocess.run([str(executable), str(trace), variant, "large_reads"], check=True)
        if variant == "complete":
            # 同一真实线路加两种持久故障：不能因短暂恢复策略而永久亮绿灯。
            for fault in ("stuck", "no_pong"):
                subprocess.run([str(executable), str(trace), variant, fault], check=True)
        encoded = Path(str(trace) + ".connect.bin").read_bytes()
        command = p.Parser().feed(encoded, 0)
        assert len(command) == 1 and len(encoded) <= 300
        gateway.receive(command[0])
        assert backend.commands[-1]["ssid"] == '网络"\\'
        assert backend.commands[-1]["password"] == '"\\' * 32
        print("F407 worst-case password JSON/CRC accepted by real camera Gateway")


if __name__ == "__main__":
    main()
