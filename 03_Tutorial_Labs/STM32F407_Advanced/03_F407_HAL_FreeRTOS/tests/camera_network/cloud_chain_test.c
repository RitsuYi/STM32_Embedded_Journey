/* 正式Camera_Task与Python网关互喂线路文件；队列/UART由既有替身提供。
 * GUI执行本身另由dashboard回归验证，这里绝不将回执替身当实机运行。 */
#define main legacy_upload_test_main
#include "upload_chain_test.c"
#undef main

static void save_frame(const char *path)
{
    FILE *file=fopen(path,"wb");assert(file);
    assert(fwrite(captured,1,captured_length,file)==captured_length);fclose(file);
}
int main(int argc,char **argv)
{
    assert(argc==5);assert(Camera_NetworkPrepare());
    bound=network.online=1;local_session=11;camera_session=99;cloud_sent=ticks;
    FILE *file=fopen(argv[1],"rb");assert(file);
    used=(uint16_t)fread(rx,1,sizeof(rx),file);fclose(file);parse(ticks);
    CameraCloudCommand command;
    assert(Camera_CloudRead(&command)&&command.token==42&&command.operation==CAMERA_CLOUD_SET);
    assert(command.mask==5&&command.values[2]==3012);
    CameraCloudReply reply={.camera_session=99,.peer_session=11,.token=42,.code=200};
    CameraCloudState state={.values={1,0,3012,2000,3000,1200,20,10},
        .total=3,.passed=2,.failed=1,.uptime_ms=ticks,.status=1,.progress=0,.trigger=1};
    Camera_CloudPublish(&state);captured_frames=0;
    Camera_CloudReply(&reply);service_cloud(ticks);
    /* 距上次状态还不到500ms，也必须先发执行后的状态再发回执。 */
    assert(captured_frames==2&&captured_types[0]==CLOUD_STATE&&captured_types[1]==CLOUD_REPLY);
    assert(captured[3]==CLOUD_REPLY&&get16(captured+20)==200);save_frame(argv[2]);
    /* 同令牌重传不能再入GUI队列，只重放相同执行结果。 */
    receive_frame(CLOUD_COMMAND,2,99,rx+12,43,ticks);
    assert(!Camera_CloudRead(&command)&&captured[3]==CLOUD_REPLY);
    /* 进入队列后等待过久会作废，2秒回执超时之后不得再执行。 */
    put32(rx+16,43);receive_frame(CLOUD_COMMAND,3,99,rx+12,43,ticks);
    ticks+=1600;assert(!Camera_CloudRead(&command));
    state.uptime_ms=ticks;state.progress=33;
    Camera_CloudPublish(&state);cloud_sent=ticks-500;service_cloud(ticks);
    assert(captured[3]==CLOUD_STATE&&get16(captured+10)==89);save_frame(argv[3]);
    CameraUploadRecord record={.record_id=123,.uptime_ms=ticks,.primary_cmm=3012,.secondary_cmm=2000,
        .rectangle=1,.verdict=1,.simulated=1,.extended=1,.trigger=1,.duration_ms=1500,
        .targets={3012,2000,3000,1200},.upper_cmm=20,.lower_cmm=10,.total=3,.passed=2,.failed=1};
    assert(Camera_UploadSubmit(&record));service_upload(ticks);
    assert(captured[3]==UPLOAD_V2&&get16(captured+10)==64);save_frame(argv[4]);
    /* 扩展上传也必须识别正确的ACK类型；旧UPLOAD类型不改变新帧的布局。 */
    const char ack[]="{\"peer_session\":11,\"request\":35,\"accepted\":true}";
    receive_frame(ACK,upload.seq,99,(const uint8_t *)ack,strlen(ack),ticks);assert(upload.acked);
    printf("PASS: cloud token dedup/expiry/reply, binary telemetry and upload-v2, unchanged 300B TX\n");
    return 0;
}
