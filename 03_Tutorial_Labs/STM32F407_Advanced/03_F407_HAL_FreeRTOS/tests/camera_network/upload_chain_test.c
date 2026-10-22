/* 正式Camera_Task参与电脑闭环；只有队列、时钟和UART由固定替身提供。
 * Python另一半运行正式Gateway/OneNET映射；云回执是明确的模拟数据。 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../User/Task/Camera_Task.c"

static uint8_t captured[300];
static unsigned captured_length;
/* 仅主机验证记录最近16帧的类型，用来检查状态帧确实先于成功回执。 */
static uint8_t captured_types[16];
static unsigned captured_frames;
static uint32_t ticks=5000;
static unsigned queue_head,queue_count;
uint32_t HAL_GetTick(void) {return ticks;}
uint32_t HAL_GetUIDw0(void) {return 1;}
uint32_t HAL_GetUIDw1(void) {return 2;}
void vTaskDelay(TickType_t delay) {ticks+=delay;}
QueueHandle_t xQueueCreateStatic(unsigned length,unsigned size,uint8_t *data,StaticQueue_t *c)
{assert(length==1 || length==4);c->data=data;c->size=size;c->full=0;return c;}
BaseType_t xQueueSend(QueueHandle_t q,const void *value,TickType_t wait)
{
    assert(wait==0);
    if(q==uploads) {
        if(queue_count==4)return 0;
        memcpy(q->data+((queue_head+queue_count)%4)*q->size,value,q->size);++queue_count;return pdPASS;
    }
    if(q->full)return 0;
    memcpy(q->data,value,q->size);q->full=1;return pdPASS;
}
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *value)
{memcpy(q->data,value,q->size);q->full=1;return pdPASS;}
BaseType_t xQueueReceive(QueueHandle_t q,void *value,TickType_t wait)
{
    assert(wait==0);
    if(q==uploads) {
        if(!queue_count)return 0;
        memcpy(value,q->data+queue_head*q->size,q->size);queue_head=(queue_head+1)%4;--queue_count;return pdPASS;
    }
    if(!q->full)return 0;
    memcpy(value,q->data,q->size);q->full=0;return pdPASS;
}
bool UART2_Bus_Init(void) {return true;}
int32_t UART2_Bus_Read(uint8_t *out,uint16_t capacity,TickType_t wait)
{(void)out;(void)capacity;ticks+=wait;return 0;}
bool UART2_Bus_Write(const uint8_t *data,uint16_t length,TickType_t wait)
{(void)wait;assert(length<=sizeof(captured));captured_types[captured_frames++%16]=data[3];memcpy(captured,data,length);captured_length=length;return true;}

int main(int argc,char **argv)
{
    assert(argc==4 || argc==6);
    bool ring=!strcmp(argv[1],"ring");
    assert(Camera_NetworkPrepare());bound=1;network.online=1;local_session=0x12345678;camera_session=99;next_seq=44;
    CameraUploadRecord record={.record_id=123,.uptime_ms=5000,.primary_cmm=ring?3012:3200,
        .secondary_cmm=ring?1206:2000,.rectangle=!ring,.verdict=ring?2:0,.simulated=1};
    /* 真实入队复制、4条容量及第5条拒绝；GUI复用原结构不影响已排入的数值。 */
    for(unsigned i=0;i<4;++i)assert(Camera_UploadSubmit(&record));
    assert(!Camera_UploadSubmit(&record) && g_camera_task.upload_dropped==1);
    record.primary_cmm=1;service_upload(ticks);
    assert(captured[3]==UPLOAD && get16(captured+4)==45 && upload.record.primary_cmm==(ring?3012:3200));
    assert(g_camera_task.upload_accepted==0 && g_camera_task.upload_state==CAMERA_UPLOAD_SENDING);
    FILE *request_file=fopen(argv[2],"wb");assert(request_file);
    assert(fwrite(captured,1,captured_length,request_file)==captured_length);fclose(request_file);
    if(argc==6) {
        FILE *reply_file=fopen(argv[4],"rb");assert(reply_file);
        unsigned n=(unsigned)fread(rx,1,sizeof(rx),reply_file);assert(feof(reply_file));fclose(reply_file);
        /* 所有应答都经过正式CRC/长度/会话入口，不能直接写状态变量伪造成功。 */
        used=(uint16_t)n;parse(ticks);
        unsigned expected=(unsigned)strtoul(argv[5],NULL,10);
        assert(g_camera_task.upload_state==expected);
        assert((g_camera_task.upload_accepted==1)==(expected==CAMERA_UPLOAD_ACCEPTED));
        if(expected==CAMERA_UPLOAD_ACCEPTED)assert(!upload.active && g_camera_task.upload_cloud_code==200);
        if(expected==CAMERA_UPLOAD_MQTT_ACKED) {
            assert(upload.active && upload.acked && g_camera_task.upload_accepted==0);
            ticks+=185001;service_upload(ticks);assert(g_camera_task.upload_state==CAMERA_UPLOAD_UNCONFIRMED);
        }
    } else {
        /* 未受理时同seq/同负载重发；超出3次仍未确认，不能记平台拒收或成功。 */
        uint8_t original[300];unsigned length=captured_length;memcpy(original,captured,length);
        for(unsigned i=0;i<2;++i) {ticks+=1001;service_upload(ticks);assert(captured_length==length && !memcmp(original,captured,length));}
        ticks+=1001;service_upload(ticks);assert(!upload.active && g_camera_task.upload_state==CAMERA_UPLOAD_UNCONFIRMED);
        /* 下条已尝试发送遇到会话中断，同样必须收口为未确认。 */
        service_upload(++ticks);assert(upload.active);drop_link();assert(!upload.active);
        /* 最坏uint32数值与大尺寸报文仍可放入原300B工作区。 */
        queue_count=0;bound=network.online=1;local_session=camera_session=UINT32_MAX;
        record=(CameraUploadRecord){.record_id=UINT32_MAX,.uptime_ms=UINT32_MAX,
            .primary_cmm=1000000,.secondary_cmm=1000000,.rectangle=1,.simulated=1,.verdict=2};
        ticks=UINT32_MAX;assert(Camera_UploadSubmit(&record));service_upload(ticks);
        assert(upload.active && captured_length<=300 && captured[3]==UPLOAD);
    }
    printf("PASS F407 upload %s: state=%" PRIu32 ", frame=%u bytes\n",argv[1],g_camera_task.upload_state,captured_length);
    return 0;
}
