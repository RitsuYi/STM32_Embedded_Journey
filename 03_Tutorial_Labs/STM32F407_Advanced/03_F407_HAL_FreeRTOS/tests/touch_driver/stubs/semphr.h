#ifndef TOUCH_TEST_SEMPHR_H
#define TOUCH_TEST_SEMPHR_H
#include "FreeRTOS.h"
typedef struct { unsigned held; } StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage);
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex);
#endif
