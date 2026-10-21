#include "FreeRTOS.h"
/* 测试只推进虚拟时间，不模拟或声称板上调度性能。 */
void vTaskDelay(TickType_t ticks);
