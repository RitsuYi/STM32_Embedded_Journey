#ifndef CAMERA_TEST_FREERTOS_H
#define CAMERA_TEST_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef struct { uint8_t *data; size_t size; int full; } StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
#define pdPASS 1
#define pdMS_TO_TICKS(n) (n)
#endif
