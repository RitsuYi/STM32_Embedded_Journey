#include "OLED.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"


void OLED_Task(void *pvParameters)
{
    OLED_ClearArea(0, 50, 128, 14);
    if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)== GPIO_PIN_RESET)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)== GPIO_PIN_RESET)
        {
            OLED_ShowString(0, 50, "KEY1 Pressed", OLED_6X8);
        }
    }

    if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)== GPIO_PIN_RESET)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)== GPIO_PIN_RESET)
        {
            OLED_ShowString(0, 50, "KEY2 Pressed", OLED_6X8);
        }
    }

    if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)== GPIO_PIN_RESET)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)== GPIO_PIN_RESET)
        {
            OLED_ShowString(0, 50, "KEY3 Pressed", OLED_6X8);
        }
    } 

    vTaskDelay(pdMS_TO_TICKS(10));
    
    OLED_Update();
}
