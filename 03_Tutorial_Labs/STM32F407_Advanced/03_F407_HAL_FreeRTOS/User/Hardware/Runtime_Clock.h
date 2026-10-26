#ifndef RUNTIME_CLOCK_H
#define RUNTIME_CLOCK_H
#include <stdint.h>
/* FreeRTOS在启动调度器时初始化一次计时器，随后在任务切换时读累计微秒。
 * 使用空闲的TIM5内部计数，不连接引脚、不新增中断，也不改变HAL的TIM6时基。 */
void RuntimeClock_Init(void);
uint64_t RuntimeClock_Read(void);
#endif
