#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f4xx.h"

void Timer_Init(void);
uint32_t Timer_GetTick(void);
void Timer_IRQHandler(void);
void Timer_RunPendingTasks(void);

#endif
