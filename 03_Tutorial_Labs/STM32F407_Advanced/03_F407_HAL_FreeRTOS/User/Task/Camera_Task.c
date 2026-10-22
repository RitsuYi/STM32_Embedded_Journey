#include "Camera_Task.h"
#include "Camera_Network.h"
#include "Camera_Upload.h"
#include "Camera_Cloud.h"
#include "../GUI/gui_version.h"
#include "UART2_Bus.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

/* v1帧最大526B。扫描条目使用短二进制负载，省去F407动态JSON树。 */
enum { HELLO=1, PING=2, HEARTBEAT=4, CONNECT=0x10, DISCONNECT=0x11,
       SCAN=0x12, UPLOAD=0x20, CLOUD_STATE=0x21, CLOUD_REPLY=0x22, UPLOAD_V2=0x23,
       CLOUD_COMMAND=0x94, ACK=0x80, HELLO_ACK=0x81, PONG=0x82,
       RESULT=0x90, SCAN_ITEM=0x91, SCAN_END=0x92, WIFI_STATE=0x93 };
typedef struct { uint8_t kind; char ssid[33], password[65]; } NetworkRequest;
static StaticQueue_t request_control, state_control;
static uint8_t request_storage[sizeof(NetworkRequest)], state_storage[sizeof(CameraNetworkState)];
static QueueHandle_t requests, states;
/* 仅增加4条紧凑记录和一个在途快照；不扩大原300B发送区或相机任务栈。 */
static StaticQueue_t upload_control;
static uint8_t upload_storage[4U * sizeof(CameraUploadRecord)];
static QueueHandle_t uploads;
/* 新增三只静态单槽，不新建任务或扩大UART缓冲。
 * cloud_current保存最近GUI快照；remote保存最近一次命令，重复包只重放回执。 */
static StaticQueue_t cloud_command_control, cloud_reply_control, cloud_state_control;
static uint8_t cloud_command_storage[sizeof(CameraCloudCommand)];
static uint8_t cloud_reply_storage[sizeof(CameraCloudReply)];
static uint8_t cloud_state_storage[sizeof(CameraCloudState)];
static QueueHandle_t cloud_commands, cloud_replies, cloud_states;
static CameraCloudState cloud_current;
static bool cloud_state_ready;
static struct { CameraCloudCommand command; uint16_t code; uint8_t active, done; } remote;
static uint32_t cloud_sent;
static struct {
    CameraUploadRecord record;
    uint32_t started, sent;
    uint16_t seq;
    uint8_t active, attempts, acked;
} upload;
static CameraNetworkState network;
static NetworkRequest pending;
static uint8_t rx[526], tx[300];
static uint16_t used, next_seq, hello_seq, command_seq, scan_seq;
static uint32_t local_session, camera_session, frame_started;
static uint32_t last_seen, last_hello, command_started, last_send;
/* 仅保存一个在途探测，不增加队列。200ms重发同一序号和8字节负载，
 * 匹配的PONG（对方原样回送）才证明TX/RX双向都通；心跳不能代替它。 */
enum { LINK_POLL_MS=200U, LINK_LOST_MS=1000U };
static uint32_t probe_started, last_probe;
static uint16_t probe_seq;
static uint8_t probe_active, hello_pending;
static uint8_t bound, attempts, scan_mask;
volatile CameraTaskStatus g_camera_task;

/* 验证UTF-8完整字符，拒绝截断、过长编码、代理码点及控制字符。
 * 显示层可以据此逐字符检查字体，不会越过SSID的32字节边界。 */
static bool valid_name(const uint8_t *p,unsigned length)
{
    for(unsigned i=0;i<length;) {
        uint32_t code=p[i++]; unsigned rest=0; uint32_t minimum=0;
        if(code<0x80) {if(code<32||code==127)return false;continue;}
        if((code&0xE0)==0xC0) {rest=1;minimum=0x80;code&=0x1F;}
        else if((code&0xF0)==0xE0) {rest=2;minimum=0x800;code&=0x0F;}
        else if((code&0xF8)==0xF0) {rest=3;minimum=0x10000;code&=7;}
        else return false;
        if(i+rest>length)return false;
        while(rest--) {if((p[i]&0xC0)!=0x80)return false;code=(code<<6)|(p[i++]&0x3F);}
        if(code<minimum||code>0x10FFFF||(code>=0xD800&&code<=0xDFFF))return false;
    }
    return true;
}

bool Camera_NetworkPrepare(void)
{
    /* 单槽请求满时立即拒绝，状态覆盖旧快照；两者均使用静态内存。 */
    if (!requests) requests=xQueueCreateStatic(1,sizeof(NetworkRequest),request_storage,&request_control);
    if (!states) states=xQueueCreateStatic(1,sizeof(CameraNetworkState),state_storage,&state_control);
    if (!uploads) uploads=xQueueCreateStatic(4,sizeof(CameraUploadRecord),upload_storage,&upload_control);
    if(!cloud_commands)cloud_commands=xQueueCreateStatic(1,sizeof(CameraCloudCommand),cloud_command_storage,&cloud_command_control);
    if(!cloud_replies)cloud_replies=xQueueCreateStatic(1,sizeof(CameraCloudReply),cloud_reply_storage,&cloud_reply_control);
    if(!cloud_states)cloud_states=xQueueCreateStatic(1,sizeof(CameraCloudState),cloud_state_storage,&cloud_state_control);
    return requests && states && uploads && cloud_commands && cloud_replies && cloud_states;
}
bool Camera_CloudRead(CameraCloudCommand *c)
{
    if(!c||!cloud_commands||xQueueReceive(cloud_commands,c,0)!=pdPASS)return false;
    /* 超过1.5秒仍未轮到GUI的命令作废；2秒超时回执之后不能再悄悄执行。 */
    return bound&&network.online&&c->peer_session==local_session&&
        c->camera_session==camera_session&&HAL_GetTick()-c->received_ms<1500U;
}
void Camera_CloudReply(const CameraCloudReply *r)
{ if(r&&cloud_replies)(void)xQueueOverwrite(cloud_replies,r); }
void Camera_CloudPublish(const CameraCloudState *s)
{ if(s&&cloud_states)(void)xQueueOverwrite(cloud_states,s); }
bool Camera_UploadSubmit(const CameraUploadRecord *record)
{
    /* 公差与尺寸已由GUI验证，这里再次检查接口边界，防止非法快照进入串口。
     * 只保存整数和标志；调用结束后GUI可复用原记录对象。 */
    bool valid=record && record->primary_cmm>0 && record->primary_cmm<=1000000 &&
        record->secondary_cmm>0 && record->secondary_cmm<=1000000 &&
        record->rectangle<=1 && record->verdict<=2 && record->simulated<=1;
    if (!valid || !uploads || xQueueSend(uploads,record,0)!=pdPASS) {
        ++g_camera_task.upload_dropped; return false;
    }
    return true;
}
static void upload_finish(CameraUploadState state)
{
    g_camera_task.upload_state=state;
    if (state==CAMERA_UPLOAD_ACCEPTED) ++g_camera_task.upload_accepted;
    else if (state==CAMERA_UPLOAD_UNCONFIRMED) ++g_camera_task.upload_unconfirmed;
    else ++g_camera_task.upload_failed;
    upload.active=0;
}
static bool request(uint8_t kind, const char *ssid, const char *password)
{
    if (kind==CONNECT && !ssid) return false;
    NetworkRequest value={0}; value.kind=kind;
    if (ssid) {
        size_t a=strlen(ssid), b=password?strlen(password):0;
        if (!a || a>32 || b>64 || !valid_name((const uint8_t *)ssid,(unsigned)a) ||
            !valid_name((const uint8_t *)password,(unsigned)b)) return false;
        for (size_t i=0;i<a;++i) if ((uint8_t)ssid[i]<32 || ssid[i]==127) return false;
        for (size_t i=0;i<b;++i) if ((uint8_t)password[i]<32 || password[i]==127) return false;
        memcpy(value.ssid,ssid,a); if (b) memcpy(value.password,password,b);
    }
    /* 立即复制凭据，不保留GUI关闭输入框后失效的密码指针。 */
    /* 断开是用户的最新意图，覆盖尚未取走的扫描/连接；单槽仍不等待GUI。 */
    bool ok=requests && (kind==DISCONNECT ? xQueueOverwrite(requests,&value) :
                                          xQueueSend(requests,&value,0))==pdPASS;
    memset(value.password,0,sizeof(value.password)); return ok;
}
bool Camera_RequestScan(void) { return request(SCAN,NULL,NULL); }
bool Camera_RequestConnect(const char *ssid,const char *password) { return request(CONNECT,ssid,password); }
bool Camera_RequestDisconnect(void) { return request(DISCONNECT,NULL,NULL); }
bool Camera_NetworkRead(CameraNetworkState *state)
{ return state && states && xQueueReceive(states,state,0)==pdPASS; }
static void publish(void) { if (states) (void)xQueueOverwrite(states,&network); }
/* 按字节访问小端整数，避免未对齐强转及C结构体填充差异。 */
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p)|(uint32_t)get16(p+2)<<16; }
static void put16(uint8_t *p,uint16_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static void put32(uint8_t *p,uint32_t n) { put16(p,(uint16_t)n); put16(p+2,(uint16_t)(n>>16)); }
static uint16_t crc16(const uint8_t *p,unsigned n)
{
    uint16_t crc=0xFFFF;
    while (n--) {
        crc^=(uint16_t)*p++<<8;
        for (unsigned i=0;i<8;++i) crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0));
    }
    return crc;
}
static bool send_frame(uint8_t kind,uint16_t seq,const uint8_t *payload,unsigned length)
{
    if (length+14>sizeof(tx)) return false;
    if (length && payload!=tx+12) memmove(tx+12,payload,length);
    tx[0]=0xAA; tx[1]=0x55; tx[2]=1; tx[3]=kind;
    put16(tx+4,seq); put32(tx+6,local_session); put16(tx+10,(uint16_t)length);
    put16(tx+12+length,crc16(tx+2,length+10));
    bool ok=UART2_Bus_Write(tx,(uint16_t)(length+14),pdMS_TO_TICKS(40));
    if (!ok) ++g_camera_task.transmit_errors;
    memset(tx,0,sizeof(tx)); return ok;
}
/* 只提取握手/ACK中的标量字段；跳过字符串转义，不把值里的伪字段当成键。
 * 输入严格按长度遍历，不依赖串口负载带字符串结束符，不申请动态内存。 */
static bool json_number(const uint8_t *p,unsigned n,const char *key,uint32_t *out)
{
    for (unsigned i=0;i<n;) {
        if (p[i++]!='"') continue;
        unsigned start=i;
        while (i<n && p[i]!='"') { if (p[i]=='\\' && i+1<n) ++i; ++i; }
        if (i>=n) return false;
        unsigned size=i-start; ++i;
        while (i<n && (p[i]==' ' || p[i]=='\r' || p[i]=='\n' || p[i]=='\t')) ++i;
        if (i>=n || p[i]!=':') continue;
        ++i; while (i<n && (p[i]==' ' || p[i]=='\t')) ++i;
        if (size!=strlen(key) || memcmp(p+start,key,size)) continue;
        uint32_t value=0; unsigned digits=0;
        if (i+4<=n && !memcmp(p+i,"true",4)) { value=1; i+=4; digits=1; }
        else if (i+5<=n && !memcmp(p+i,"false",5)) { i+=5; digits=1; }
        else while (i<n && p[i]>='0' && p[i]<='9') {
            unsigned digit=p[i++]-'0';
            if (value>(UINT32_MAX-digit)/10U) return false;
            value=value*10U+digit; ++digits;
        }
        while (i<n && (p[i]==' ' || p[i]=='\t')) ++i;
        if (!digits || i>=n || (p[i]!=',' && p[i]!='}')) return false;
        *out=value; return true;
    }
    return false;
}
static void drop_link(void)
{
    /* 已经尝试发送的记录不能跨新会话自动重发，否则可能把同一件再上报一次。
     * 未拿到最终回执时如实记未确认，云端可能已经收到，需按业务编号核对。 */
    if (upload.active && upload.attempts) upload_finish(CAMERA_UPLOAD_UNCONFIRMED);
    bound=0; camera_session=0; pending.kind=0; probe_active=0; hello_pending=0;
    remote.active=0;cloud_sent=HAL_GetTick()-1000U;
    last_hello=HAL_GetTick()-LINK_POLL_MS; /* 掉线后下一轮立即发现，不额外等周期。 */
    memset(pending.password,0,sizeof(pending.password));
    network.online=0; network.wifi=CAMERA_WIFI_UNKNOWN; network.count=0;
    network.ssid[0]=network.ip[0]=0;
    if (network.scan==CAMERA_SCAN_RUNNING || network.scan==CAMERA_SCAN_WAITING) network.scan=CAMERA_SCAN_FAILED;
    network.error=CAMERA_NET_LINK_LOST; g_camera_task.online=0; publish();
}
static void receive_frame(uint8_t kind,uint16_t seq,uint32_t session,
                          const uint8_t *p,unsigned n,uint32_t now)
{
    uint32_t peer,value;
    if (!session) return;
    if (!bound && kind==HELLO_ACK && seq==hello_seq && json_number(p,n,"peer_session",&peer) &&
        peer==local_session && json_number(p,n,"bound",&value)) {
        if (!value) {
            /* 相机提供新随机会话号，避免F407重启复用旧会话；不改PLL/RNG配置。 */
            uint32_t suggested;
            if (!json_number(p,n,"suggested_session",&suggested) || !suggested) return;
            camera_session=session; local_session=suggested; last_seen=now; last_hello=now-LINK_POLL_MS;
            hello_pending=0; /* 发现转为确认时换序号，同一阶段重发则保留原序号。 */
        } else if (value==1 && session==camera_session) {
            /* 握手只建立会话，首次匹配PONG再显示已连接。
             * 否则只有HELLO能通过、探测始终失败时，会每秒先绿后灰地循环。 */
            bound=1; last_seen=now;
            probe_active=0; hello_pending=0; last_probe=now-LINK_POLL_MS;
        }
        return;
    }
    /* 握手被拒绝时重新发现，避免一直向旧挑战/旧相机启动号确认。 */
    if (!bound && kind==ACK && seq==hello_seq &&
        json_number(p,n,"peer_session",&peer) && peer==local_session) {
        camera_session=0; hello_pending=0; last_hello=now; return;
    }
    if (!bound || session!=camera_session) {
        if (bound && kind==HEARTBEAT && session!=camera_session) drop_link();
        return;
    }
    if (kind==PONG) {
        if (probe_active && seq==probe_seq && n==8 && get32(p)==local_session &&
            get32(p+4)==probe_started && now-probe_started<LINK_LOST_MS) {
            last_seen=now; probe_active=0;
            ++g_camera_task.matched_pongs;
            if (!network.online) {
                network.online=1; network.error=CAMERA_NET_OK;
                g_camera_task.online=1; publish();
            }
        }
        return; /* 旧序号、错误负载或其他会话的应答不能续在线。 */
    }
    if (kind==PING && n<=32) { (void)send_frame(PONG,seq,p,n); return; }
    if(kind==CLOUD_COMMAND&&n==43&&get32(p)==local_session&&network.online) {
        CameraCloudCommand c={.camera_session=session,.peer_session=local_session,
            .token=get32(p+4),.received_ms=now,.operation=p[8],.mask=get16(p+9)};
        for(unsigned i=0;i<8;++i)c.values[i]=get32(p+11+4*i);
        if(!c.token)return;
        if(remote.active&&remote.command.token==c.token) {
            /* 同token异内容拒绝；缓存中的相同命令绝不重复开始或停止。 */
            bool same=remote.command.operation==c.operation&&remote.command.mask==c.mask&&
                !memcmp(remote.command.values,c.values,sizeof(c.values));
            if(remote.done||!same) {
                uint8_t reply[10];put32(reply,camera_session);put32(reply+4,c.token);
                put16(reply+8,same?remote.code:422);(void)send_frame(CLOUD_REPLY,seq,reply,sizeof(reply));
            }
        } else if(!remote.active||remote.done) {
            if(cloud_commands&&xQueueSend(cloud_commands,&c,0)==pdPASS) {
                remote.command=c;remote.active=1;remote.done=0;
            }
        }
        return;
    }
    if (upload.active && upload.attempts && seq==upload.seq &&
        json_number(p,n,"peer_session",&peer) && peer==local_session) {
        if (kind==ACK && json_number(p,n,"request",&value) && (value==UPLOAD||value==UPLOAD_V2) &&
            json_number(p,n,"accepted",&value)) {
            if (value==1) upload.acked=1;
            else upload_finish(CAMERA_UPLOAD_REJECTED);
            return;
        }
        uint32_t record_id,state,terminal;
        if (kind==RESULT && json_number(p,n,"record_id",&record_id) &&
            record_id==upload.record.record_id && json_number(p,n,"upload_state",&state) &&
            state>=CAMERA_UPLOAD_QUEUED && state<=CAMERA_UPLOAD_FAILED &&
            json_number(p,n,"terminal",&terminal) && terminal<=1) {
            uint32_t code=0,ok=0;
            (void)json_number(p,n,"cloud_code",&code);
            /* 平台接受还需明确ok=true/code=200，不能只信任一个状态数字。 */
            if (state==CAMERA_UPLOAD_ACCEPTED && (!terminal || code!=200 ||
                !json_number(p,n,"ok",&ok) || ok!=1)) return;
            if (!terminal && state>=CAMERA_UPLOAD_ACCEPTED) return;
            upload.acked=1;g_camera_task.upload_state=state;g_camera_task.upload_cloud_code=code;
            if (terminal) upload_finish(state>=CAMERA_UPLOAD_ACCEPTED?(CameraUploadState)state:
                                       CAMERA_UPLOAD_UNCONFIRMED);
            return;
        }
    }
    if (kind==WIFI_STATE && n>=8 && get32(p)==local_session && p[4]<=CAMERA_WIFI_CONNECTED &&
        p[5]<=32 && p[6]<=15 && n==8U+p[5]+p[6] && valid_name(p+8,p[5])) {
        network.wifi=p[4];
        memcpy(network.ssid,p+8,p[5]); network.ssid[p[5]]=0;
        memcpy(network.ip,p+8+p[5],p[6]); network.ip[p[6]]=0;
        if (p[7] && network.error==CAMERA_NET_OK) network.error=CAMERA_NET_REJECTED;
        /* p[7]是本次Wi-Fi快照的错误标志，不是永久故障锁存。
         * 后台恢复后清除旧拒绝/超时；扫描失败仍由自己的完成结果负责，
         * 不能用一个正常Wi-Fi心跳掩盖列表缺项或扫描失败。 */
        else if (!p[7] && network.scan!=CAMERA_SCAN_FAILED &&
                 (network.error==CAMERA_NET_REJECTED || network.error==CAMERA_NET_TIMEOUT))
            network.error=CAMERA_NET_OK;
        publish(); return;
    }
    if (kind==ACK && pending.kind && seq==command_seq && json_number(p,n,"peer_session",&peer) &&
        peer==local_session && json_number(p,n,"accepted",&value)) {
        if (!value) {
            network.error=CAMERA_NET_REJECTED;
            if (pending.kind==SCAN) network.scan=CAMERA_SCAN_FAILED;
        }
        /* 受理不是完成，扫描仍等END，Wi-Fi仍等实际状态。 */
        pending.kind=0; memset(pending.password,0,sizeof(pending.password)); publish(); return;
    }
    if (kind==SCAN_ITEM && seq==scan_seq && network.scan==CAMERA_SCAN_RUNNING && n>=8 &&
        get32(p)==local_session && p[4]<CAMERA_AP_LIMIT && p[6]<=1 &&
        p[7]>0 && p[7]<=32 && n==8U+p[7]) {
        unsigned i=p[4];
        if (!valid_name(p+8,p[7])) return;
        memcpy(network.ap[i].ssid,p+8,p[7]); network.ap[i].ssid[p[7]]=0;
        network.ap[i].rssi=(int8_t)p[5]; network.ap[i].secured=p[6];
        scan_mask|=(uint8_t)(1U<<i); return;
    }
    if (kind==SCAN_END && seq==scan_seq && network.scan==CAMERA_SCAN_RUNNING &&
        n==6 && get32(p)==local_session && p[4]<=CAMERA_AP_LIMIT) {
        uint8_t expected=(uint8_t)((1U<<p[4])-1U);
        bool complete=p[5]==0 && scan_mask==expected;
        network.count=complete?p[4]:0; network.scan=complete?CAMERA_SCAN_DONE:CAMERA_SCAN_FAILED;
        network.error=complete?CAMERA_NET_OK:(p[5]?CAMERA_NET_SCAN_ERROR:CAMERA_NET_INCOMPLETE);
        pending.kind=0; publish(); return;
    }
    if (kind==RESULT && seq==command_seq && json_number(p,n,"peer_session",&peer) &&
        peer==local_session && json_number(p,n,"ok",&value) && !value) {
        network.error=CAMERA_NET_REJECTED; publish();
    }
}
/* 坏头/坏CRC只前移一字节；半帧留在唯一静态数组中，下次Read继续填充。 */
static void parse(uint32_t now)
{
    while (used>=2) {
        if (rx[0]!=0xAA || rx[1]!=0x55 || (used>=12 && (rx[2]!=1 || get16(rx+10)>512))) {
            memmove(rx,rx+1,--used); frame_started=now; continue;
        }
        if (used<12) return;
        unsigned length=get16(rx+10), total=length+14;
        if (used<total) return;
        if (crc16(rx+2,length+10)==get16(rx+12+length)) {
            ++g_camera_task.received_frames;
            receive_frame(rx[3],get16(rx+4),get32(rx+6),rx+12,length,now);
            used=(uint16_t)(used-total); memmove(rx,rx+total,used);
        } else { ++g_camera_task.frame_errors; memmove(rx,rx+1,--used); }
        frame_started=now;
    }
}
static unsigned quote(uint8_t *out,const char *text)
{
    unsigned n=0; out[n++]='"';
    while (*text) { if (*text=='"' || *text=='\\') out[n++]='\\'; out[n++]=(uint8_t)*text++; }
    out[n++]='"'; return n;
}
static void send_command(void)
{
    /* 最坏双倍转义小于300B；先检查32B/64B长度，才能安全使用这个固定数组。 */
    unsigned n=(unsigned)snprintf((char *)tx+12,sizeof(tx)-12,"{\"target_session\":%" PRIu32,camera_session);
    if (pending.kind==CONNECT) {
        memcpy(tx+12+n,",\"ssid\":",8); n+=8; n+=quote(tx+12+n,pending.ssid);
        memcpy(tx+12+n,",\"password\":",12); n+=12; n+=quote(tx+12+n,pending.password);
    }
    tx[12+n++]='}'; (void)send_frame(pending.kind,command_seq,tx+12,n);
}
/* 每轮只比较毫秒计数，到期才发18/22字节小帧；等待收包由既有Read休眠。
 * unsigned减法允许HAL毫秒计数回绕，不创建定时器、任务或动态协议内存。 */
static void service_link(uint32_t now)
{
    if (bound && now-last_seen>=LINK_LOST_MS) {
        ++g_camera_task.link_timeouts; drop_link();
    }
    if (!bound && camera_session && now-last_seen>=1500U) {camera_session=0;hello_pending=0;}
    if (!bound && now-last_hello>=LINK_POLL_MS) {
        uint8_t target[4]; put32(target,camera_session);
        /* 重发保持同一序号，延迟超过200ms的应答仍能匹配，不因加快重试而饿死握手。 */
        if (!hello_pending) {hello_seq=++next_seq;hello_pending=1;}
        (void)send_frame(HELLO,hello_seq,target,4); last_hello=now;
    }
    if (bound && now-last_probe>=LINK_POLL_MS) {
        uint8_t payload[8];
        if (!probe_active) {probe_seq=++next_seq;probe_started=now;probe_active=1;}
        put32(payload,local_session); put32(payload+4,probe_started);
        (void)send_frame(PING,probe_seq,payload,sizeof(payload)); last_probe=now;
    }
}
static void service_upload(uint32_t now)
{
    /* 最多一个在途，只有Camera任务调用send_frame；GUI与网络线程不碰UART。
     * 串口受理最多重发3次且保持seq/负载，相机缓存应答防止重复执行。
     * ACK之后只等业务结果，最长185秒覆盖相机180秒有界缓存及返回延迟。 */
    if (!upload.active && uploads && xQueueReceive(uploads,&upload.record,0)==pdPASS) {
        upload.active=1;upload.attempts=0;upload.acked=0;upload.started=now;
        g_camera_task.upload_record_id=upload.record.record_id;
        g_camera_task.upload_state=CAMERA_UPLOAD_QUEUED;g_camera_task.upload_cloud_code=0;
    }
    if (!upload.active) return;
    if (now-upload.record.uptime_ms>=185000U || now-upload.started>=185000U) {
        upload_finish(upload.attempts?CAMERA_UPLOAD_UNCONFIRMED:CAMERA_UPLOAD_FAILED);return;
    }
    if (!bound || !network.online || upload.acked) return;
    if (upload.attempts && now-upload.sent<1000U) return;
    if (upload.attempts>=3) {upload_finish(CAMERA_UPLOAD_UNCONFIRMED);return;}
    if (!upload.attempts) upload.seq=++next_seq;
    /* 固定设备名与已验证的短枚举无需动态JSON树；snprintf检查避免截断发包。
     * 不发送残差：当前模拟后端未实际计算，不能把固定零当成精度证据。 */
    const CameraUploadRecord *r=&upload.record;
    if(r->extended) {
        /* 小端紧凑快照共64B；不扩大发送区/任务栈，也不把完整JSON搬入F407。
         * 参数与统计随每件一起传，不能依赖稍后收到的状态来补历史。 */
        uint8_t *p=tx+12;put32(p,camera_session);put32(p+4,r->record_id);
        put32(p+8,r->uptime_ms);put32(p+12,(uint32_t)r->primary_cmm);put32(p+16,(uint32_t)r->secondary_cmm);
        p[20]=r->rectangle;p[21]=r->verdict;p[22]=r->simulated;p[23]=r->trigger;
        put32(p+24,r->duration_ms);
        for(unsigned i=0;i<4;++i)put32(p+28+4*i,r->targets[i]);
        put32(p+44,r->upper_cmm);put32(p+48,r->lower_cmm);
        put32(p+52,r->total);put32(p+56,r->passed);put32(p+60,r->failed);
        (void)send_frame(UPLOAD_V2,upload.seq,p,64);
        upload.sent=now;++upload.attempts;g_camera_task.upload_state=CAMERA_UPLOAD_SENDING;return;
    }
    static const char *const verdicts[]={"NONE","PASS","FAIL"};
    int n=snprintf((char *)tx+12,sizeof(tx)-14,
        "{\"target_session\":%" PRIu32 ",\"record\":{\"device_id\":\"VM_F407_001\","
        "\"record_id\":%" PRIu32 ",\"geometry\":\"%s\",\"primary_cmm\":%" PRId32
        ",\"secondary_cmm\":%" PRId32 ",\"verdict\":\"%s\",\"source\":\"%s\",\"uptime_ms\":%" PRIu32 "}}",
        camera_session,r->record_id,r->rectangle?"rectangle":"ring",r->primary_cmm,
        r->secondary_cmm,verdicts[r->verdict],r->simulated?"simulated":"camera",r->uptime_ms);
    if (n<0 || (unsigned)n>=sizeof(tx)-14) {upload_finish(CAMERA_UPLOAD_FAILED);return;}
    (void)send_frame(UPLOAD,upload.seq,tx+12,(unsigned)n);
    upload.sent=now;++upload.attempts;g_camera_task.upload_state=CAMERA_UPLOAD_SENDING;
}
static void service_cloud(uint32_t now)
{
    CameraCloudReply reply;
    if(cloud_replies&&xQueueReceive(cloud_replies,&reply,0)==pdPASS&&remote.active&&
       reply.token==remote.command.token&&reply.camera_session==camera_session&&
       reply.peer_session==local_session) {remote.code=reply.code;remote.done=1;}
    if(remote.active&&!remote.done&&now-remote.command.received_ms>=2000U) {
        remote.code=504;remote.done=1;
    }
    /* 必须在取回执之后再取快照：GUI先投状态再投回执，避免两个任务交错时
     * 先取到了旧状态、后取到了新回执，导致成功后仍向网页上报旧模式。 */
    if(cloud_states&&xQueueReceive(cloud_states,&cloud_current,0)==pdPASS)cloud_state_ready=true;
    bool replying=bound&&remote.active&&remote.done==1;
    if(bound&&network.online&&cloud_state_ready&&now-cloud_current.uptime_ms<1500U&&
       (replying||now-cloud_sent>=500U)) {
        /* 普通状态每500ms一次；执行回执前立即发送新快照，省掉下一周期等待。
         * UART保持同一发送者，帧顺序固定为状态→回执，负载大小不变。 */
        uint8_t *p=tx+12;memset(p,0,89);put32(p,camera_session);
        for(unsigned i=0;i<8;++i)put32(p+4+4*i,cloud_current.values[i]);
        put32(p+36,cloud_current.total);put32(p+40,cloud_current.passed);put32(p+44,cloud_current.failed);
        put32(p+48,cloud_current.uptime_ms);put16(p+52,cloud_current.error);
        p[54]=cloud_current.status;p[55]=cloud_current.progress;p[56]=cloud_current.trigger;
        /* 版本来自原唯一版本头，不另造一套与设置页冲突的固件号。 */
        (void)snprintf((char *)p+57,32,"%s",GUI_FIRMWARE_VERSION);
        if(send_frame(CLOUD_STATE,++next_seq,p,89))cloud_sent=now;
    }
    /* 完成回执的同token缓存保留；值2表示已发，重试只重放、不重复执行。 */
    if(replying) {
        uint8_t p[10];put32(p,camera_session);put32(p+4,remote.command.token);put16(p+8,remote.code);
        if(send_frame(CLOUD_REPLY,++next_seq,p,sizeof(p)))remote.done=2;
    }
}
void vCamera_Task(void *argument)
{
    (void)argument;
    uint8_t bytes[128];
    /* 正式启动号来自相机的随机挑战，此处仅生成发现阶段的临时标识。 */
    local_session=HAL_GetUIDw0()^HAL_GetUIDw1()^HAL_GetTick();
    if (!local_session) local_session=1;
    last_hello=HAL_GetTick()-LINK_POLL_MS;
    for (;;) {
        uint32_t now=HAL_GetTick();
        if (!UART2_Bus_Init()) {
            g_camera_task.ready=0; ++g_camera_task.receive_errors; used=0;
            /* 本地DMA恢复不等于对端已离线。恢复期间仍检查原一秒期限，
             * 短暂错误不切换图标；持续恢复失败也不能无限保留在线状态。 */
            service_link(HAL_GetTick());
            vTaskDelay(pdMS_TO_TICKS(200)); continue;
        }
        g_camera_task.ready=1;
        if (used && now-frame_started>=500U) {
            memmove(rx,rx+1,--used); ++g_camera_task.frame_errors; frame_started=now; parse(now);
        }
        /* 空闲时在流缓冲上休眠最多20ms，减少无数据唤醒；来字节即唤醒。
         * 200ms探测/1000ms失联期限不变，GUI请求最多多等一个20ms读周期。 */
        int32_t count=UART2_Bus_Read(bytes,sizeof(bytes),pdMS_TO_TICKS(20));
        if (count<0) {
            used=0; ++g_camera_task.receive_errors; g_camera_task.ready=0;
            /* 丢弃残帧，让下轮Init恢复接收；保留会话和当前探测，
             * 后续有效PONG可继续同一连接，不为一个坏字节重新握手。 */
            service_link(HAL_GetTick());
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        now=HAL_GetTick();
        if (count>0) {
            g_camera_task.received_bytes+=(uint32_t)count; g_camera_task.last_byte=bytes[count-1];
            /* 一次复制一段，再解析其中的完整帧，避免每个字节都重新检查帧头。
             * 只用原静态526B接收数组；残帧、粘包和坏帧仍交给同一parse处理。 */
            unsigned offset=0;
            while (offset<(unsigned)count) {
                if (!used) frame_started=now;
                if (used==sizeof(rx)) { memmove(rx,rx+1,--used); ++g_camera_task.frame_errors; }
                unsigned chunk=(unsigned)count-offset,space=sizeof(rx)-used;
                if(chunk>space)chunk=space;
                memcpy(rx+used,bytes+offset,chunk);used=(uint16_t)(used+chunk);offset+=chunk;
                parse(now);
            }
        }
        service_link(now);
        NetworkRequest next;
        if (xQueueReceive(requests,&next,0)==pdPASS) {
            if (next.kind==SCAN) { network.scan=CAMERA_SCAN_WAITING; network.count=0; scan_mask=0; }
            else if (network.scan==CAMERA_SCAN_RUNNING || network.scan==CAMERA_SCAN_WAITING) {
                network.scan=CAMERA_SCAN_IDLE; network.count=0;
            }
            if (!bound && next.kind!=SCAN) network.error=CAMERA_NET_LINK_LOST;
            else {
                pending=next; command_started=now; attempts=0; network.error=CAMERA_NET_OK;
                if (bound) {
                    command_seq=++next_seq;
                    if (pending.kind==SCAN) { scan_seq=command_seq; network.scan=CAMERA_SCAN_RUNNING; }
                }
            }
            memset(next.password,0,sizeof(next.password)); publish();
        }
        if (pending.kind && bound) {
            if (pending.kind==SCAN && network.scan==CAMERA_SCAN_WAITING) {
                command_seq=++next_seq; scan_seq=command_seq; network.scan=CAMERA_SCAN_RUNNING;
                command_started=now; attempts=0; publish();
            }
            if (!attempts || now-last_send>=1000U) { send_command(); last_send=now; ++attempts; }
            if (attempts>=3 && now-last_send>=900U) {
                if (pending.kind==SCAN) network.scan=CAMERA_SCAN_FAILED;
                pending.kind=0; memset(pending.password,0,sizeof(pending.password));
                network.error=CAMERA_NET_TIMEOUT; publish();
            }
        }
        if ((network.scan==CAMERA_SCAN_RUNNING || network.scan==CAMERA_SCAN_WAITING) &&
            now-command_started>=5000U) {
            network.scan=CAMERA_SCAN_FAILED; network.error=CAMERA_NET_TIMEOUT; pending.kind=0; publish();
        }
        service_upload(now);
        service_cloud(now);
        /* 连续数据也每轮让出1ms，避免优先级3的Camera任务长期压住GUI/OLED。 */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
