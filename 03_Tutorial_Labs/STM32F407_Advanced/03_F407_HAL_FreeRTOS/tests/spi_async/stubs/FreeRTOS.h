#ifndef SPI_ASYNC_MODEL_RTOS_H
#define SPI_ASYNC_MODEL_RTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef void *TaskHandle_t;
typedef struct {int value,depth;} StaticSemaphore_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define portYIELD_FROM_ISR(woken) ((void)(woken))
#endif
