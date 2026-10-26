#include "ST7796.h"
#include "GUI_Task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "lvgl_port.h"
#include "gui_app.h"
#include "gui_lvgl_assert.h"
#include "FT6336U.h"
#include "Camera_Network.h"
#include "Camera_Upload.h"
#include "Camera_Task.h"
#include "System_Monitor.h"

static bool upload_record(void *context,const InstrumentRecord *record)
{
    (void)context;
    /* GUI拥有模型，复制成普通整数快照后即返回。普通测量为0未判定，
     * 质检不合格统一映射2；不能把应用内部的失败原因枚举直接发给物模型。 */
    CameraUploadRecord value={.record_id=record->id,.uptime_ms=record->uptime_ms,
        .primary_cmm=record->result.primary_cmm,.secondary_cmm=record->result.secondary_cmm,
        .rectangle=record->result.geometry==MEASUREMENT_RECTANGLE,
        .verdict=record->inspection?(record->verdict==INSPECTION_PASS?1:2):0,
        .simulated=record->result.simulated};
    GUI_AppUploadSnapshot(&value);
    return Camera_UploadSubmit(&value);
}

/* GUI仅投递普通请求；扫码/串口/DMA等待由Camera任务和相机已有子进程处理。 */
static bool network_scan(void *context) {(void)context;return Camera_RequestScan();}
static bool network_connect(void *context,const char *ssid,const char *password)
{(void)context;return Camera_RequestConnect(ssid,password);}
static bool network_disconnect(void *context) {(void)context;return Camera_RequestDisconnect();}

static StaticQueue_t command_queue_storage;
static uint8_t command_queue_data[8U * sizeof(unsigned)];
static QueueHandle_t command_queue;

bool GUI_TaskPrepare(void)
{
    if(!command_queue) command_queue = xQueueCreateStatic(8, sizeof(unsigned),
                                           command_queue_data, &command_queue_storage);
    return command_queue != NULL;
}
bool GUI_PostKey(unsigned key)
{
    if(!command_queue || xQueueSend(command_queue, &key, 0) != pdPASS) {
        return false;
    }
    return true;
}
void GUI_LVGL_AssertFailed(const char *file, int line)
{
    /* 保留断言失败时的停机保护，防止内存异常后继续访问失效对象。 */
    (void)file; (void)line;
    taskDISABLE_INTERRUPTS();
    for(;;) { }
}

/* 将硬件驱动的结果交给现有LVGL输入接口；context本工程不需要，所以忽略。
 * 只有GUI任务会调用它，其他任务不碰LVGL；失败时仍明确上报松手。 */
static bool read_touch(void *context, int32_t *x, int32_t *y, bool *pressed)
{
    (void)context;
    FT6336U_Point point;
    bool valid = FT6336U_Read(&point);
    *x = point.x;
    *y = point.y;
    *pressed = point.pressed;
    return valid;
}

/*
 * @brief GUI任务
 */
void vGUI_Task(void *pvParameters)
{
    (void)pvParameters;

    ST7796_Init();

    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    if(!GUI_DisplayInit()) GUI_LVGL_AssertFailed(__FILE__, __LINE__);
    /* 接线已由用户确认。失败不停止显示：驱动在读回调中每秒重试。
     * 输入设备注册失败则是LVGL内存问题，与硬件未应答分别处理。 */
    (void)FT6336U_Init();
    if (!GUI_RegisterTouch(read_touch, NULL)) GUI_LVGL_AssertFailed(__FILE__, __LINE__);
    /* 在用户打开系统信息前就开始轻量CPU采样，首屏直接使用最近结果。 */
    SystemMonitor_Poll();
    GUI_AppInit(NULL); /* Flash persistence not connected: RAM-only, explicitly labelled. */
    GUI_AppSetSystemSource(SystemMonitor_Read,NULL);
    GUI_AppSetUpload(upload_record,NULL);
    const DashboardNetworkOps network_ops={.connect=network_connect,.disconnect=network_disconnect,.scan=network_scan};
    GUI_AppSetNetwork(&network_ops);

    for (;;)
    {
        /* 内部限制每秒一次，只读计数；任务栈仍仅在系统信息可见时采集。 */
        SystemMonitor_Poll();
        unsigned key;
        /* Camera任务只入队；真正校验/改参数/开始/停止全部在这里执行。
         * 回执带原会话号，掉线换会话后的旧命令不得误作用于新连接。 */
        CameraCloudCommand cloud;
        bool cloud_taken=Camera_CloudRead(&cloud);
        CameraCloudReply reply={0};
        if(cloud_taken) {
            reply=(CameraCloudReply){.camera_session=cloud.camera_session,
                .peer_session=cloud.peer_session,.token=cloud.token,.code=503};
            if(g_camera_task.online)reply.code=GUI_AppCloudCommand(&cloud);
        }
        CameraCloudState cloud_state;GUI_AppCloudState(&cloud_state);
        Camera_CloudPublish(&cloud_state);
        /* 先投递执行后的状态，再投递回执；相机收到成功时已有新模式/运行状态。
         * 两者都是原静态单槽，整个过程不增加任务，也不等待云端。 */
        if(cloud_taken)Camera_CloudReply(&reply);
        /* 每轮只取最新快照；队列没有数据立即返回，不等待相机或网络。 */
        CameraNetworkState network_state;
        if(Camera_NetworkRead(&network_state)) GUI_AppNetworkUpdate(&network_state);
        /* Bound work per iteration so queued input cannot starve rendering. */
        for(unsigned i = 0; i < 8 && xQueueReceive(command_queue, &key, 0) == pdPASS; ++i)
            GUI_AppKey(key);
        GUI_AppProcess();
        uint32_t delay = lv_timer_handler();
        GUI_DisplayService(); /* 刷新失败后的完整重绘请求在本次LVGL处理结束后提交。 */
        if(delay == LV_NO_TIMER_READY || delay > 10U) delay = 10U;
        if(delay < 1U) delay = 1U;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

