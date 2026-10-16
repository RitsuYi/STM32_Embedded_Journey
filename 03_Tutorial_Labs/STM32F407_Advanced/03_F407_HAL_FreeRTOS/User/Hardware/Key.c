#include "Key.h"
#include "Key_Task.h"

#include "main.h"

#include "FreeRTOS.h"
#include "task.h"


void vKeyTask(void *pvParameters)
{
    GPIO_PinState key1Previous = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
    GPIO_PinState key2Previous = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);
    GPIO_PinState key3Previous = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);

    GPIO_PinState key1Current;
    GPIO_PinState key2Current;
    GPIO_PinState key3Current;


    for (;;)
    {
        /*
         * 读取 KEY1
         * RESET = 按下
         * SET   = 松开
         */
        key1Current = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
        key2Current = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);
        key3Current = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);

        if ((key1Current != key1Previous) &&
            (key1Current == GPIO_PIN_SET))
        {
            KEY1_Task();
        }

        if ((key2Current != key2Previous) &&
            (key2Current == GPIO_PIN_SET))
        {
            KEY2_Task();
        }

        if ((key3Current != key3Previous) &&
            (key3Current == GPIO_PIN_SET))
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