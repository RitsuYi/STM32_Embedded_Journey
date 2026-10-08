#include "Key.h"
#include "Key_Task.h"

#include "main.h"

#include "FreeRTOS.h"
#include "task.h"


void vKeyTask(void *pvParameters)
{
    uint8_t key1Previous = 0;
    uint8_t key2Previous = 0;
    uint8_t key3Previous = 0;

    uint8_t key1Current;
    uint8_t key2Current;
    uint8_t key3Current;


    for (;;)
    {
        /*
         * 读取 KEY1
         * RESET = 按下
         * SET   = 松开
         *
         * 转换：
         * 1 = 按下
         * 0 = 松开
         */
        key1Current =
            (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)
             == GPIO_PIN_RESET);


        key2Current =
            (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)
             == GPIO_PIN_RESET);


        key3Current =
            (HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)
             == GPIO_PIN_RESET);


        /*
         * KEY1 按下
         */
        if ((key1Previous == 0) &&
            (key1Current == 1))
        {
            KEY1_Task();
        }


        /*
         * KEY2 按下
         */
        if ((key2Previous == 0) &&
            (key2Current == 1))
        {
            KEY2_Task();
        }


        /*
         * KEY3 按下
         */
        if ((key3Previous == 0) &&
            (key3Current == 1))
        {
            KEY3_Task();
        }


        /*
         * 保存当前状态
         */
        key1Previous = key1Current;
        key2Previous = key2Current;
        key3Previous = key3Current;


        /*
         * 10ms 扫描周期
         */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}