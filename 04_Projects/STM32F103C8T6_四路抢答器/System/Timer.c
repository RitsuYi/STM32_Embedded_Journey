#include "stm32f10x.h"
#include "Timer.h"

static volatile uint32_t TimerMillis;

void Timer_Init(void)
{
	TimerMillis = 0;
	SysTick_Config(SystemCoreClock / 1000UL);
}

uint32_t Timer_GetMillis(void)
{
	return TimerMillis;
}

void Timer_TickISR(void)
{
	TimerMillis++;
}

