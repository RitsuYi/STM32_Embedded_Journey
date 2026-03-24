#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"
#include "Encoder.h"

int16_t Speed;

int main(void)
{
	OLED_Init();
	Timer_Init();
	Encoder_Init();
		
	while (1)
	{
		
	}
}

void TIM2_IRQHandler(void)
{
    
}
