#include "FreeRTOS.h"
#define taskSCHEDULER_RUNNING 1
BaseType_t xTaskGetSchedulerState(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
TickType_t xTaskGetTickCount(void);
