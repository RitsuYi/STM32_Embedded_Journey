#include "stm32f10x.h"                  // Device header
#include "OLED.h"

/*妯″紡4*/
void Mode4_Init(void)
{
	OLED_Clear();
	OLED_ShowString(1, 1, "Model4");
}

void Mode4_Loop(void)
{
}

void Mode4_Exit(void)
{
}

void Mode4_Tick(void)
{
}
