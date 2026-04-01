#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Key.h"
#include "LED.h"

uint8_t KeyNum;
uint8_t FlashFlag;
uint16_t i;

int main(void)
{
	OLED_Init();
	LED_Init();
	Key_Init();
	
	while (1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1)
		{
			FlashFlag = !FlashFlag;
			LED1_Turn();
		}

		if(FlashFlag)
		{
			LED1_ON();
			Delay_ms(500);
			LED1_OFF();
			Delay_ms(500);
		}
		else
		{
			LED1_OFF();
		}

		OLED_ShowNum(1, 1, i ++, 5);
	}
}

void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2,TIM_IT_Update) == SET)
	{

		TIM_ClearITPendingBit(TIM2,TIM_IT_Update);
	}
}
