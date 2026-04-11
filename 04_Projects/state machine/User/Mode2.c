#include "stm32f10x.h"                  // Device header
#include "OLED.h"

/*模式2*/
void Mode2_Init(void)
{
	OLED_Clear();
	OLED_ShowString(1, 1, "Model2");
}

void Mode2_Loop(void)
{
}

void Mode2_Exit(void)
{
}

void Mode2_Tick(void)
{
}
