#include "stm32f4xx.h"                  // Device header
#include "OLED.h"
#include "LED.h"
#include "Delay.h"


static void OLED_ShowTestPattern(void)
{
	OLED_Clear();
	OLED_DrawRectangle(0, 0, 128, 64, OLED_UNFILLED);
	OLED_DrawLine(0, 0, 127, 63);
	OLED_DrawLine(127, 0, 0, 63);
	OLED_ShowString(28, 8, "OLED", OLED_8X16);
	OLED_ShowString(12, 32, "PB8 SCL  PB9 SDA", OLED_6X8);
	OLED_ShowString(18, 44, "ADDR 0x78", OLED_6X8);
	OLED_Update();
}

int main(void)
{	
	OLED_Init();
	
	while(1)
	{
		LED1_ON();
		OLED_DrawRectangle(0, 0, 128, 64, OLED_FILLED);
		OLED_Update();
		Delay_ms(300);

		LED1_OFF();
		OLED_ShowTestPattern();
		Delay_ms(700);
	}
}


