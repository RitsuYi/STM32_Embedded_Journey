#include "OLED_Task.h"
#include "lvgl_port.h"

/* 只在OLED任务内排版：GUI只发布数字，不在渲染期间等待I²C。
 * 上方32像素显示最近约1秒的实际完成帧率、末帧耗时/块数及累计传输错误。
 * 所有数字直接用整数绘制，避免小任务栈使用浮点printf及大字符串缓冲。 */
static void show_display_stats(const GUI_DisplayStats *stats)
{
    uint32_t fps = stats->fps_x10 > 9999U ? 9999U : stats->fps_x10;
    OLED_ClearArea(0, 0, 128, 32);
    OLED_ShowString(0, 0, "FPS:", OLED_8X16);
    OLED_ShowNum(32, 0, fps / 10U, 3, OLED_8X16);
    OLED_ShowChar(56, 0, '.', OLED_8X16);
    OLED_ShowNum(64, 0, fps % 10U, 1, OLED_8X16);
    OLED_ShowString(0, 16, "MS:", OLED_6X8);
    OLED_ShowNum(18, 16, stats->last_frame_ms > 9999U ? 9999U : stats->last_frame_ms, 4, OLED_6X8);
    OLED_ShowString(48, 16, "BLK:", OLED_6X8);
    OLED_ShowNum(72, 16, stats->last_frame_blocks > 999U ? 999U : stats->last_frame_blocks, 3, OLED_6X8);
    OLED_ShowString(0, 24, "ERR:", OLED_6X8);
    OLED_ShowNum(24, 24, stats->transfer_errors % 1000000U, 6, OLED_6X8);
    OLED_ShowString(66, 24, "1s avg", OLED_6X8);
    OLED_UpdateArea(0, 0, 128, 32);
}

void vOLED_Task(void *pvParameters)
{
    (void)pvParameters;
    uint32_t last_stats_ms = HAL_GetTick();
    unsigned last_key = 4U; /* 0为未按下；4让第一次循环也清理旧按键提示。 */
    GUI_DisplayStats stats;
    /* 清除启动文字一次，此后只发变化的区域，不再每轮发送1024B整屏。
     * OLED与触摸共用100kHz I²C，原来的整屏轮询会长期占用总线。 */
    OLED_ClearArea(0, 0, 128, 48);
    OLED_UpdateArea(0, 0, 128, 48);
    for(;;)
    {
        uint32_t now = HAL_GetTick();
        if(now - last_stats_ms >= 1000U) {
            /* 临界区只复制36字节统计；不在这里绘字、延时或发送I²C。
             * 暂停任务切换可确保FPS、耗时和块数来自同一份GUI统计。 */
            taskENTER_CRITICAL();
            GUI_DisplayGetStats(&stats);
            taskEXIT_CRITICAL();
            show_display_stats(&stats);
            last_stats_ms = now;
        }

        unsigned key = 0U;
        if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)== GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)== GPIO_PIN_RESET)
            {
                key = 1U;
            }
        }

        if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)== GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)== GPIO_PIN_RESET)
            {
                key = 2U;
            }
        }

        if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)== GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)== GPIO_PIN_RESET)
            {
                key = 3U;
            }
        } 
        /* 保留按键提示及原10ms消抖；只在按下/松手变化时发送底部两页数据。
         * 统计区域每秒最多刷新一次，页与页之间仍按原驱动释放I²C共用锁。 */
        if(key != last_key) {
            OLED_ClearArea(0, 50, 128, 14);
            if(key == 1U) OLED_ShowString(0, 50, "KEY1 Pressed", OLED_8X16);
            else if(key == 2U) OLED_ShowString(0, 50, "KEY2 Pressed", OLED_8X16);
            else if(key == 3U) OLED_ShowString(0, 50, "KEY3 Pressed", OLED_8X16);
            OLED_UpdateArea(0, 50, 128, 14);
            last_key = key;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        
    }
}
