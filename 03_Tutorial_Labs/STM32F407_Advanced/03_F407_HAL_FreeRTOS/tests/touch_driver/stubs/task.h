#ifndef TOUCH_TEST_TASK_H
#define TOUCH_TEST_TASK_H
#include "FreeRTOS.h"
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
#define taskSCHEDULER_SUSPENDED 2
BaseType_t xTaskGetSchedulerState(void);
void vTaskDelay(TickType_t ticks);
/* 主机只有一个执行线程；临界区替身用于检查驱动的发布路径，不模拟中断时序。 */
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
