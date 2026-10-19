#include "FreeRTOS.h"
typedef StaticSemaphore_t *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *s);
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *s);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s,TickType_t timeout);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t timeout);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *woken);
