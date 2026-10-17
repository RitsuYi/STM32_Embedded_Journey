#ifndef TOUCH_TEST_FREERTOS_H
#define TOUCH_TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#endif
