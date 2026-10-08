#include "FreeRTOS.h"
#include "semphr.h"
#include "Buzzer.h"

static SemaphoreHandle_t xBuzzerSemphr;

void vBuzzer_Task(void *pvParameters)
{
    xBuzzerSemphr = xSemaphoreCreateBinary();

    for (;;)
    {
        xSemaphoreTake(xBuzzerSemphr, portMAX_DELAY);
        Buzzer_On();
        vTaskDelay(pdMS_TO_TICKS(100));
        Buzzer_Off();
    }

}

void Buzzer_Beep(void)
{
    xSemaphoreGive(xBuzzerSemphr);
}
