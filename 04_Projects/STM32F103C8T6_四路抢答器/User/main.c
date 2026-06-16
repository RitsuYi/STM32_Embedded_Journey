#include "stm32f10x.h"
#include "OLED.h"
#include "Key.h"
#include "LED.h"
#include "Buzzer.h"
#include "Timer.h"
#include "Responder.h"

int main(void)
{
	uint32_t nowMs;
	uint32_t keyScanTime;
	
	OLED_Init();
	LED_Init();
	Key_Init();
	Buzzer_Init();
	Responder_Init();
	Timer_Init();
	
	keyScanTime = Timer_GetMillis();
	
	while (1)
	{
		nowMs = Timer_GetMillis();
		
		if ((uint32_t)(nowMs - keyScanTime) >= 10)
		{
			keyScanTime = nowMs;
			Key_Update();
		}
		
		Responder_Update(nowMs);
		Buzzer_Update(nowMs);
	}
}

