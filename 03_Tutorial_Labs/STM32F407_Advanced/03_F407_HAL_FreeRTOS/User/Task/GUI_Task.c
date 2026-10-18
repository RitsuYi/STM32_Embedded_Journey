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
    GUI_AppInit(NULL); /* Flash persistence not connected: RAM-only, explicitly labelled. */

    for (;;)
    {
        unsigned key;
        /* Bound work per iteration so queued input cannot starve rendering. */
        for(unsigned i = 0; i < 8 && xQueueReceive(command_queue, &key, 0) == pdPASS; ++i)
            GUI_AppKey(key);
        GUI_AppProcess();
        uint32_t delay = lv_timer_handler();
        if(delay == LV_NO_TIMER_READY || delay > 10U) delay = 10U;
        if(delay < 1U) delay = 1U;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

