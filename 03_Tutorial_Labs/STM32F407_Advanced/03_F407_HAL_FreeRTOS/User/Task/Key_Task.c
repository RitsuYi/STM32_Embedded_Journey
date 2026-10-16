#include "Key_Task.h"
#include "Buzzer_Task.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "GUI_Task.h"

extern TimerHandle_t xTimerForLED1;
uint8_t blinkMode = 0;
uint32_t periods[] = {pdMS_TO_TICKS(500), pdMS_TO_TICKS(200), pdMS_TO_TICKS(100)};

void KEY1_Task(void)
{
    (void)GUI_PostKey(1);
    Buzzer_Beep();
    if (xTimerIsTimerActive(xTimerForLED1) != pdFALSE)
    {
        xTimerStop(xTimerForLED1, portMAX_DELAY);
    }
    else
    {
        xTimerStart(xTimerForLED1, portMAX_DELAY);
    }
}


void KEY2_Task(void)
{
    (void)GUI_PostKey(2);
    Buzzer_Beep();
    blinkMode = (blinkMode + 1) % 3;
    xTimerChangePeriod(xTimerForLED1, periods[blinkMode], portMAX_DELAY);
}


void KEY3_Task(void)
{
    (void)GUI_PostKey(3);
    Buzzer_Beep();
    xTimerReset(xTimerForLED1, portMAX_DELAY);                  
}
