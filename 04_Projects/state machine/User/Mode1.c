#include "stm32f10x.h"                  // Device header
#include "OLED.h"

/*模式1*/

void Mode1_Init(void)
{
	OLED_Clear();
	OLED_ShowString(1, 1, "Model1");
}

void Mode1_Loop(void)
{
}

void Mode1_Exit(void)
{
}

void Mode1_Tick(void)
{
}
