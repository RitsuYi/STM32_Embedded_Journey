#include "OLED_Task.h"

void vOLED_Task(void *pvParameters)
{
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
        OLED_Update();
        vTaskDelay(pdMS_TO_TICKS(10));
        
    }
}
