#include <stdint.h>
/* 当前SPI等待接口以系统节拍计时；主机只提供等宽类型，不模拟RTOS调度。 */
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(value) (value)
