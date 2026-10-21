/* 编译真正Camera_Task.c，只有HAL串口/队列/时间由电脑替代。
 * 输入输出来自真实Python Gateway与WifiScanner产生的二进制轨迹。 */
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../User/Task/Camera_Task.c"
static uint32_t ticks;
static jmp_buf finished;
static FILE *trace;
static unsigned exchanges, expected_exchanges, chunk;
static uint8_t incoming[4096];
static unsigned available, position;
static bool capture;
static uint8_t captured[300];
static unsigned captured_size;
static unsigned probes;
/* 故障注入覆盖真实任务循环：一次收包错误、随后100ms本地恢复，
 * 对端仍能应答。连续跑5秒，检查GUI快照是否被瞬时错误误报离线。 */
static bool injected_rx_error, premature_online;
static uint32_t rx_recover_until;
static unsigned transient_offline;
static bool stuck_receiver, suppress_pongs, large_reads;
static uint32_t first_timeout_age;
/* 同一正式接收入口先收到错误、再收到健康快照：旧错误必须随恢复清除。
 * 扫描失败独立于是否联网，不能因后来一个正常心跳就丢掉缺项提示。 */
static void wifi_error_recovery_contract(void)
{
    CameraNetworkState saved=network;
    uint8_t state[8]={0};put32(state,local_session);
    network.scan=CAMERA_SCAN_IDLE;network.error=CAMERA_NET_OK;
    state[4]=CAMERA_WIFI_UNKNOWN;state[7]=1;
    receive_frame(WIFI_STATE,0,camera_session,state,sizeof(state),ticks);
    assert(network.error==CAMERA_NET_REJECTED);
    state[4]=CAMERA_WIFI_CONNECTED;state[7]=0;
    receive_frame(WIFI_STATE,0,camera_session,state,sizeof(state),ticks);
    assert(network.error==CAMERA_NET_OK && network.online);
    network.error=CAMERA_NET_TIMEOUT;
    state[4]=CAMERA_WIFI_OFFLINE;
    receive_frame(WIFI_STATE,0,camera_session,state,sizeof(state),ticks);
    assert(network.error==CAMERA_NET_OK && network.wifi==CAMERA_WIFI_OFFLINE);
    network.scan=CAMERA_SCAN_FAILED;network.error=CAMERA_NET_INCOMPLETE;
    receive_frame(WIFI_STATE,0,camera_session,state,sizeof(state),ticks);
    assert(network.scan==CAMERA_SCAN_FAILED && network.error==CAMERA_NET_INCOMPLETE);
    network=saved;publish();
    puts("PASS: current Wi-Fi health clears stale errors and retains independent scan failure");
}
uint32_t HAL_GetTick(void) { return ticks; }
uint32_t HAL_GetUIDw0(void) { return 0x10; }
uint32_t HAL_GetUIDw1(void) { return 0x20; }
void vTaskDelay(TickType_t wait)
{
    ticks+=wait;
    if(ticks>=5000 && exchanges==expected_exchanges)longjmp(finished,1);
    assert(ticks<7000); /* 握手失败不能靠无限重试把测试挂住。 */
}
QueueHandle_t xQueueCreateStatic(unsigned length,unsigned size,uint8_t *data,StaticQueue_t *c)
{ assert(length==1); c->data=data;c->size=size;c->full=0;return c; }
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t wait)
{ assert(wait==0);if(q->full)return 0;memcpy(q->data,item,q->size);q->full=1;return pdPASS; }
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *item)
{
    if(q==states) {
        const CameraNetworkState *state=item;
        if(state->online && !probes)premature_online=true;
        if(injected_rx_error && ticks<2000 && !state->online)++transient_offline;
        if(!state->online && g_camera_task.link_timeouts && !first_timeout_age)
            first_timeout_age=ticks-last_seen;
    }
    memcpy(q->data,item,q->size);q->full=1;return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait)
{assert(wait==0);if(!q->full)return 0;memcpy(item,q->data,q->size);q->full=0;return pdPASS;}
bool UART2_Bus_Init(void)
{return ticks>=rx_recover_until && !(stuck_receiver && injected_rx_error);}
int32_t UART2_Bus_Read(uint8_t *out,uint16_t capacity,TickType_t wait)
{
    if(!suppress_pongs && !injected_rx_error && ticks>=1200) {
        injected_rx_error=true;rx_recover_until=ticks+100;return -1;
    }
    unsigned count=available-position;
    /* 碎片模式每轮1~29字节；大块模式尽量读满128B，覆盖一次解析多个帧。 */
    unsigned limit=large_reads?capacity:1U+(chunk++%29U);
    if(count>limit)count=limit;
    if(count>capacity)count=capacity;
    ticks+=count?1U:wait; /* 已有数据立即取，空闲才按调用方给定期限休眠。 */
    memcpy(out,incoming+position,count);position+=count;
    return (int32_t)count;
}
bool UART2_Bus_Write(const uint8_t *data,uint16_t length,TickType_t wait)
{
    (void)wait;
    if(capture) {assert(length<=sizeof(captured));memcpy(captured,data,length);captured_size=length;return true;}
    if(data[3]==PING) {
        /* 线路轨迹外的周期探测独立回送，检查实际CRC/8字节挑战。
         * Python侧另用正式Gateway验证PING/PONG编码，避免把两端同一替身当证据。 */
        assert(length==22 && get16(data+20)==crc16(data+2,18));
        ++probes;
        if(suppress_pongs || (stuck_receiver && injected_rx_error))return true;
        assert(available+length<=sizeof(incoming));
        uint8_t *reply=incoming+available;memcpy(reply,data,length);
        reply[3]=PONG;put32(reply+6,camera_session);put16(reply+20,crc16(reply+2,18));
        available+=length;return true;
    }
    /* 永久接收故障/完全没有探测回包时，新的发现也不再得到应答。
     * 这部分模拟真正的断链，不读取原正常握手/扫描轨迹之外的数据。 */
    if(exchanges==expected_exchanges && data[3]==HELLO)return true;
    uint8_t head[4], expected[300];
    assert(fread(head,1,4,trace)==4);
    unsigned request_length=get16(head), reply_length=get16(head+2);
    assert(request_length==length && length<=sizeof(expected));
    assert(fread(expected,1,length,trace)==length);
    if(memcmp(expected,data,length)) {
        fprintf(stderr,"wire mismatch exchange %u kind=%u seq=%u\n",exchanges,data[3],get16(data+4));abort();
    }
    /* UART可同时收发；保留尚未读完的数据，再追加下一条真实回包。 */
    unsigned remaining=available-position;
    assert(remaining+reply_length<=sizeof(incoming));
    memmove(incoming,incoming+position,remaining);
    assert(fread(incoming+remaining,1,reply_length,trace)==reply_length);
    available=remaining+reply_length;position=0;++exchanges;return true;
}
int main(int argc,char **argv)
{
    assert(argc==3 || argc==4);trace=fopen(argv[1],"rb");assert(trace);
    stuck_receiver=argc==4 && !strcmp(argv[3],"stuck");
    suppress_pongs=argc==4 && !strcmp(argv[3],"no_pong");
    large_reads=argc==4 && !strcmp(argv[3],"large_reads");
    expected_exchanges=4;
    assert(Camera_NetworkPrepare());
    assert(Camera_RequestScan());
    assert(!Camera_RequestScan()); /* 单槽满立即拒绝，无等待/无限队列。 */
    if(setjmp(finished)==0)vCamera_Task(NULL);
    assert(exchanges==4);assert(fgetc(trace)==EOF);fclose(trace);
    assert(!premature_online); /* 握手完成后仍需首次匹配PONG才能显示绿灯。 */
    if(!stuck_receiver && !suppress_pongs)assert(injected_rx_error && transient_offline==0);
    CameraNetworkState result;
    assert(Camera_NetworkRead(&result));assert(!Camera_NetworkRead(&network));
    if(stuck_receiver || suppress_pongs) {
        /* 恢复失败也必须如期离线，不能靠延长显示时间遮住实际断链。
         * 完全无PONG时，握手成功也从未发出online快照。 */
        assert(!result.online && !bound && g_camera_task.link_timeouts==1);
        assert(first_timeout_age>=1000 && first_timeout_age<=1200);
        if(suppress_pongs)assert(g_camera_task.matched_pongs==0);
        else assert(g_camera_task.matched_pongs>0);
        printf("Camera task %s: offline at virtual last-PONG age %" PRIu32 "ms\n",argv[3],first_timeout_age);
        return 0;
    }
    assert(result.online && result.wifi==CAMERA_WIFI_CONNECTED);
    assert(g_camera_task.link_timeouts==0 && g_camera_task.matched_pongs>=20);
    if(!strcmp(argv[2],"complete")) {
        assert(result.scan==CAMERA_SCAN_DONE && result.count==5);
        assert(result.ap[0].rssi==-10 && !strcmp(result.ap[0].ssid,"AP19"));
    } else {
        assert(result.scan==CAMERA_SCAN_FAILED && result.count==0);
        assert(result.error==CAMERA_NET_INCOMPLETE);
    }
    wifi_error_recovery_contract();
    /* 实际F407编码最坏64B密码转义，让Python相机真实解析，检查字符不会丢失。 */
    assert(!valid_name((const uint8_t *)"\xED\xA0\x80",3));
    assert(!valid_name((const uint8_t *)"\xF0\xA0",2));
    assert(!Camera_RequestConnect("bad\nname","test"));
    char password[65];for(unsigned i=0;i<64;++i)password[i]=(i&1U)?'\\':'"';password[64]=0;
    assert(Camera_RequestConnect("网络\"\\",password));
    assert(xQueueReceive(requests,&pending,0)==pdPASS);command_seq=44;capture=true;send_command();
    char path[1024];snprintf(path,sizeof(path),"%s.connect.bin",argv[1]);
    FILE *encoded=fopen(path,"wb");assert(encoded);assert(fwrite(captured,1,captured_size,encoded)==captured_size);fclose(encoded);
    assert(probes>=22 && probes<=26); /* 5秒约25次；包含100ms恢复，不按任务循环频率发送。 */
    /* 相机已连接时Wi-Fi可独立离线，真实接收入口不能把online一起清零。 */
    uint8_t off[8]={0};put32(off,local_session);off[4]=CAMERA_WIFI_OFFLINE;
    receive_frame(WIFI_STATE,0,camera_session,off,sizeof(off),ticks);
    assert(network.online && network.wifi==CAMERA_WIFI_OFFLINE);
    /* 请求队列满时断开覆盖扫描；接下来验证旧PONG与单向心跳不能续在线。 */
    assert(Camera_RequestScan());assert(Camera_RequestDisconnect());
    assert(xQueueReceive(requests,&pending,0)==pdPASS && pending.kind==DISCONNECT);
    probe_active=0;last_probe=ticks-LINK_POLL_MS;last_seen=ticks;
    service_link(ticks);assert(captured[3]==PING);
    uint8_t nonce[8];memcpy(nonce,captured+12,sizeof(nonce));
    receive_frame(PONG,(uint16_t)(probe_seq+1),camera_session,nonce,8,ticks+100);
    assert(last_seen==ticks && probe_active);
    receive_frame(PONG,probe_seq,camera_session,nonce,8,ticks+100);
    assert(last_seen==ticks+100 && !probe_active);
    uint32_t confirmed=last_seen;
    receive_frame(HEARTBEAT,1,camera_session,NULL,0,confirmed+999);
    service_link(confirmed+999);assert(bound);
    service_link(confirmed+1000);assert(!bound && !network.online);
    /* 毫秒计数回绕也保持相同的1秒阈值。 */
    bound=1;network.online=1;last_seen=UINT32_MAX-400U;last_probe=500;
    service_link(500);assert(bound);service_link(600);assert(!bound);
    hello_pending=0;camera_session=0;last_hello=ticks-LINK_POLL_MS;
    service_link(ticks);uint16_t retry_seq=get16(captured+4);
    service_link(ticks+800);assert(captured[3]==HELLO && get16(captured+4)==retry_seq);
    /* 序号/目标会话错误不能混入列表，断线必须清除联网图标。 */
    uint8_t invalid[9]={0};put32(invalid,local_session+1);
    receive_frame(SCAN_ITEM,scan_seq,camera_session,invalid,sizeof(invalid),ticks);
    drop_link();assert(!network.online && !network.count && !network.ip[0]);
    assert(crc16((const uint8_t *)"123456789",9)==0x29B1);
    printf("Camera task wire trace %s passed; frames=%" PRIu32 ", errors=%" PRIu32 "\n",argv[2],g_camera_task.received_frames,g_camera_task.frame_errors);
    return 0;
}
