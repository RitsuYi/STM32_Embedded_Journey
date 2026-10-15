#include "ST7796.h"

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief GUI任务
 */
void vGUI_Task(void *pvParameters)
{
    (void)pvParameters;
    ST7796_Init();
    // 后续：lv_init();
    // 后续：配置LVGL显示设备和缓冲区

    for (;;)
    {
        // 后续：lv_timer_handler();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
