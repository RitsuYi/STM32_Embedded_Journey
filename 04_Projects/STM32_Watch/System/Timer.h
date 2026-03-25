#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x.h"

void Timer_Init(void);
uint32_t Timer_GetMillis(void);
void Timer_ResetMillis(void);
void Timer_IRQHandler(void);

#endif
