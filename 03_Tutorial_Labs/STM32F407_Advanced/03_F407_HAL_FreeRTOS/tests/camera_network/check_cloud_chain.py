"""正式Python下行→正式F407解析/队列/回执→Python事件上报的电脑契约。"""
import argparse
import copy
import json
import struct
import subprocess
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--camera",type=Path,required=True)
    args=parser.parse_args();sys.path[:0]=[str(args.camera),str(args.camera/"tests")]
    from gateway import protocol as p
    from gateway.cloud_protocol import command_payload,decode_state,decode_upload
    from gateway.config import validate_record,DEFAULTS
    from gateway.core import Gateway
    from gateway.cloud import OneNetBridge
    from gateway.mqtt_client import MqttUploader
    from test_gateway import Clock,FakeNetwork,bind
    from test_cloud import CloudTests
    output=ROOT/"build/Camera_UART_Check"
    request,reply,state,upload=[output/("cloud_"+x+".bin") for x in ("request","reply","state","upload")]
    request.write_bytes(p.Frame(p.CLOUD_COMMAND,1,99,command_payload(11,42,1,5,[1,0,3012,0,0,0,0,0])).encode())
    exe=output/"cloud_chain_test.exe"
    subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-Werror",
        "-I"+str(ROOT/"tests/camera_network/stubs"),"-I"+str(ROOT/"User/Hardware"),"-I"+str(ROOT/"User/Task"),
        str(ROOT/"tests/camera_network/cloud_chain_test.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe),str(request),str(reply),str(state),str(upload)],check=True)
    result,=p.Parser().feed(reply.read_bytes(),0);assert struct.unpack("<IIH",result.payload)==(99,42,200)
    packet,=p.Parser().feed(state.read_bytes(),0);target,snapshot=decode_state(packet.payload)
    assert target==99 and snapshot["values"][2]==3012 and snapshot["progress"]==33
    packet,=p.Parser().feed(upload.read_bytes(),0);data=decode_upload(packet.payload)
    normalized=validate_record(data["record"],11)
    assert normalized["details"]["targets"][0]==3012 and normalized["details"]["trigger"]==1
    test=CloudTests();test.setUp();bridge,mqtt,client=test.bridge,test.mqtt,test.client
    bridge.command(dict(op="telemetry",peer=11,state=snapshot))
    bridge.upload(dict(op="upload",peer=11,seq=packet.seq,key=("VM_F407_001",11,123),record=normalized))
    mqtt.tick();prop,event=(json.loads(x[1]) for x in client.publishes)
    assert event["params"]["measurement_completed"]["value"]["width_mm"]==30.12
    assert event["params"]["measurement_completed"]["value"]["record_id"]==prop["params"]["measurement_id"]["value"]
    mqtt._reply(bridge.base+"event/post/reply",p.json_bytes({"id":event["id"],"code":200}))
    mqtt._reply(bridge.base+"property/post/reply",p.json_bytes({"id":prop["id"],"code":200}))
    assert bridge.event_accepted==1 and not mqtt.pending
    (output/"cloud_measurement_event.json").write_text(json.dumps(event,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("PASS: Python/F407 real binary layouts and extended measurement property+event confirmations; no hardware/cloud")

if __name__=="__main__":main()
