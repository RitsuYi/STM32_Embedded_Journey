#ifndef TOUCH_TEST_TASK_H
#define TOUCH_TEST_TASK_H
#include "FreeRTOS.h"
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
#define taskSCHEDULER_SUSPENDED 2
BaseType_t xTaskGetSchedulerState(void);
void vTaskDelay(TickType_t ticks);
#endif
