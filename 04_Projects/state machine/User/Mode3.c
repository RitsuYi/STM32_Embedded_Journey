#include "stm32f10x.h"                  // Device header
#include "OLED.h"

/*模式3*/
void Mode3_Init(void)
{
	OLED_Clear();
	OLED_ShowString(1, 1, "Model3");
}

void Mode3_Loop(void)
{
}

void Mode3_Exit(void)
{
}

void Mode3_Tick(void)
{
}
