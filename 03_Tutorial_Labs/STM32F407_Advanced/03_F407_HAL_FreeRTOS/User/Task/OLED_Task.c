#include "OLED_Task.h"
#include "FT6336U.h"
#include "lvgl_port.h" /* 仅读取GUI留下的计数，OLED任务不调用任何LVGL函数。 */

#if FT6336U_OLED_DIAGNOSTICS
/* H6.5临时显示已有诊断数据，免去用户在调试器中反复停机观察。
 * 本函数只改OLED显存，仍由原有OLED_Update统一发送，不额外查询触摸芯片。
 * 小屏用英文缩写：R原始坐标，XY横屏坐标，N手指数，P按下，S通信状态，
 * OK成功帧数，ER通信失败，OUT坐标越界，ID芯片身份，RD初始化就绪，
 * LP/LR/LC是LVGL已产生的按下/松手/点击次数，与驱动P区分。
 * raw/XY松手后保留最后一点，P会变0；实际间隔/事件仍可在g_ft6336u观察。
 * 只观察实时状态，不是跨任务一致性快照；长按时可稳定读数。
 * 使用原有逐位显示函数，不使用printf或局部大缓冲，照顾OLED任务512B栈。 */
static void show_touch_diagnostics(void)
{
    OLED_ClearArea(0, 0, 128, 48);
    OLED_ShowString(0, 0, "R:", OLED_6X8);
    OLED_ShowNum(12, 0, g_ft6336u.raw_x, 4, OLED_6X8);
    OLED_ShowString(36, 0, ",", OLED_6X8);
    OLED_ShowNum(42, 0, g_ft6336u.raw_y, 4, OLED_6X8);
    OLED_ShowString(72, 0, "N", OLED_6X8);
    OLED_ShowNum(78, 0, g_ft6336u.point_count, 1, OLED_6X8);
    OLED_ShowString(90, 0, "P", OLED_6X8);
    OLED_ShowNum(96, 0, g_ft6336u.pressed, 1, OLED_6X8);

    OLED_ShowString(0, 10, "XY:", OLED_6X8);
    OLED_ShowNum(18, 10, g_ft6336u.x, 3, OLED_6X8);
    OLED_ShowString(36, 10, ",", OLED_6X8);
    OLED_ShowNum(42, 10, g_ft6336u.y, 3, OLED_6X8);
    OLED_ShowString(72, 10, "S:", OLED_6X8);
    OLED_ShowNum(84, 10, g_ft6336u.last_status, 1, OLED_6X8);

    /* 计数占6位，超过999999时显示最低6位；不要把回绕显示误当作清零。 */
    OLED_ShowString(0, 20, "OK:", OLED_6X8);
    OLED_ShowNum(18, 20, g_ft6336u.read_count, 6, OLED_6X8);
    OLED_ShowString(66, 20, "ER:", OLED_6X8);
    OLED_ShowNum(84, 20, g_ft6336u.error_count, 6, OLED_6X8);

    OLED_ShowString(0, 30, "OUT:", OLED_6X8);
    OLED_ShowNum(24, 30, g_ft6336u.out_of_range_count, 5, OLED_6X8);
    OLED_ShowString(60, 30, "ID:", OLED_6X8);
    OLED_ShowHexNum(78, 30, g_ft6336u.chip_id, 2, OLED_6X8);
    OLED_ShowString(96, 30, "RD:", OLED_6X8);
    OLED_ShowNum(114, 30, g_ft6336u.ready, 1, OLED_6X8);

    /* 最后一行专看LVGL事件链，显示各计数最低3位，单次点击通常各增加1。
     * 若驱动P正常但LP不增，应查输入命中；LP增而LR/LC不增，应查松手/取消过程。 */
    OLED_ShowString(0, 40, "LP:", OLED_6X8);
    OLED_ShowNum(18, 40, g_gui_touch.pressed_events, 3, OLED_6X8);
    OLED_ShowString(42, 40, "LR:", OLED_6X8);
    OLED_ShowNum(60, 40, g_gui_touch.released_events, 3, OLED_6X8);
    OLED_ShowString(84, 40, "LC:", OLED_6X8);
    OLED_ShowNum(102, 40, g_gui_touch.clicked_events, 3, OLED_6X8);
}
#endif

void vOLED_Task(void *pvParameters)
{
    (void)pvParameters;
    for(;;)
    {
        OLED_ClearArea(0, 50, 128, 14);
        if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)== GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)== GPIO_PIN_RESET)
            {
                OLED_ShowString(0, 50, "KEY1 Pressed", OLED_8X16);
            }
        }

        if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)== GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)== GPIO_PIN_RESET)
            {
                OLED_ShowString(0, 50, "KEY2 Pressed", OLED_8X16);
            }
        }

        if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)== GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)== GPIO_PIN_RESET)
            {
                OLED_ShowString(0, 50, "KEY3 Pressed", OLED_8X16);
            }
        } 
#if FT6336U_OLED_DIAGNOSTICS
        /* 保留底部原按键提示；临时用上方48像素定位当前触摸漏点。 */
        show_touch_diagnostics();
#endif
        OLED_Update();
        vTaskDelay(pdMS_TO_TICKS(10));
        
    }
}
