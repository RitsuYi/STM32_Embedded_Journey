#ifndef SYSTEM_TEST_TASK_H
#define SYSTEM_TEST_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
typedef struct {
    TaskHandle_t xHandle;const char *pcTaskName;
    UBaseType_t xTaskNumber,uxCurrentPriority,eCurrentState;
    uint64_t ulRunTimeCounter;
    StackType_t *pxStackBase,*pxTopOfStack,*pxEndOfStack;
    size_t usStackHighWaterMark;
} TaskStatus_t;
UBaseType_t uxTaskGetSystemState(TaskStatus_t *tasks,UBaseType_t capacity,uint64_t *total);
UBaseType_t uxTaskGetNumberOfTasks(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
TaskHandle_t xTaskGetIdleTaskHandle(void);
/* 调度器/空闲任务替身只供主机统计模型使用，不进入ARM固件。 */
#define taskSCHEDULER_RUNNING 2
int xTaskGetSchedulerState(void);
uint64_t ulTaskGetIdleRunTimeCounter(void);
#endif
