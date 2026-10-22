"""正式两端业务代码的电脑闭环。Paho/串口为替身，不连接云端或实际设备。"""
import argparse
import copy
import json
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
    from gateway.mqtt_client import MqttUploader
    from test_gateway import Clock, FakeNetwork
    from test_mqtt import FakeClient
    import types

    output = ROOT / "build/Camera_UART_Check"
    executable = output / "upload_chain_test.exe"
    subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "tests/camera_network/stubs"),
                    "-I" + str(ROOT / "User/Hardware"), "-I" + str(ROOT / "User/Task"),
                    str(ROOT / "tests/camera_network/upload_chain_test.c"), "-o", str(executable)], check=True)
    for variant in ("rectangle", "ring"):
        request_file = output / ("upload_" + variant + ".bin")
        subprocess.run([str(executable), variant, str(request_file), "encode"], check=True)
        frame, = p.Parser().feed(request_file.read_bytes(), 0)
        record = p.json_object(frame.payload)["record"]
        assert record["source"] == "simulated" and record["device_id"] == "VM_F407_001"
        clock, network, sent = Clock(), FakeNetwork(), []
        config = copy.deepcopy(DEFAULTS)
        config["mqtt"].update(enabled=True, payload_format="onenet", publish_topic="post", reply_topic="reply")
        gateway = Gateway(99, network, sent.append, clock, config)
        with patch("gateway.core.os.urandom", return_value=struct.pack("<I", frame.session)):
            gateway.receive(p.Frame(p.HELLO, 1, 0x30, struct.pack("<I", 0)))
        gateway.receive(p.Frame(p.HELLO, 2, frame.session, struct.pack("<I", 99)))
        sent.clear(); gateway.receive(frame)
        assert p.json_object(p.Parser().feed(sent[0], 0)[0].payload)["accepted"]
        client = FakeClient()
        module = types.SimpleNamespace(Client=lambda **_: client, MQTTv311=4, MQTTv31=3,
                                       topic_matches_sub=lambda a, b: a == b)
        uploader = MqttUploader(config["mqtt"], gateway.network_event, clock, module)
        uploader.enqueue(network.commands[-1]);uploader.set_wifi(True);uploader.tick()
        payload = json.loads(client.publishes[0][1]);identity = payload["id"]
        assert identity.isdigit() and len(identity) <= 13
        values = {key: value["value"] for key, value in payload["params"].items()}
        assert values["measurement_id"].startswith("sim-VM_F407_001-")
        assert values["inspection_result"] == (2 if variant == "ring" else 0)
        assert values["outer_diameter_mm" if variant == "ring" else "width_mm"] == (30.12 if variant == "ring" else 32.0)
        client.publishes[0][-1].published = True;uploader.tick()
        for state, code in ((3, None), (4, 200), (5, 2403)):
            if code is not None:
                # 第二个拒绝场景用独立上传器复现，不把已完成请求再次改判。
                if state == 5:
                    uploader = MqttUploader(config["mqtt"], gateway.network_event, clock, module)
                    uploader.enqueue(network.commands[-1]);uploader.set_wifi(True);uploader.tick()
                    identity = json.loads(client.publishes[-1][1])["id"]
                uploader._reply("reply", p.json_bytes({"id": identity, "code": code}))
            result_frames = [x for x in p.Parser().feed(b"".join(sent), 0) if x.kind == p.RESULT]
            result = result_frames[-1]
            data = p.json_object(result.payload)
            assert data["upload_state"] == state and data["record_id"] == 123
            # 先注入错误seq与错误会话的成功帧，再送真实结果，检验F407过滤。
            fake = dict(data, upload_state=4, terminal=True, ok=True, cloud_code=200)
            # 伪回包只需必要字段，保持三包总长小于正式526B接收容量。
            fake = {key: fake[key] for key in ("peer_session", "record_id", "upload_state", "terminal", "ok", "cloud_code")}
            replies = (p.Frame(p.RESULT, frame.seq + 1, 99, p.json_bytes(fake)).encode()
                       + p.Frame(p.RESULT, frame.seq, 100, p.json_bytes(fake)).encode() + result.encode())
            reply_file = output / ("upload_{}_{}.reply.bin".format(variant, state))
            reply_file.write_bytes(replies)
            assert len(replies) < 526
            subprocess.run([str(executable), variant, str(request_file), "decode", str(reply_file), str(state)], check=True)
        # 同一记录重发只回放结果，不能再次给联网队列排入上传命令。
        count = len(network.commands);gateway.receive(frame);assert len(network.commands) == count
        (output / ("upload_" + variant + ".onejson.json")).write_text(
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("PASS: real F407 -> camera Gateway -> OneJSON -> simulated cloud reply -> F407; no live cloud used")


if __name__ == "__main__":
    main()
