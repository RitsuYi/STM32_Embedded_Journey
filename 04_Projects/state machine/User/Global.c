#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "LED.h"
#include "Key.h"
#include "ModeStore.h"
#include "Timer.h"

/*全局模式*/
extern volatile uint8_t NextMode;

void Global_Init(void)
{
	OLED_Init();
	LED_Init();
	Key_Init();
	NextMode = ModeStore_Load(MODE_STORE_DEFAULT_MODE);
	
	Timer_Init();
}

void Global_Loop(void)
{
	uint8_t Key1Long;
	//uint8_t Key2Long;

	Key1Long = Key_Check(KEY_1, KEY_LONG);
	//Key2Long = Key_Check(KEY_2, KEY_LONG);
	
	if (Key1Long)
	{
		NextMode ++;
		if (NextMode > MODE_STORE_MAX_MODE)
		{
			NextMode = MODE_STORE_MIN_MODE;
		}
	}
}

void Global_Tick(void)
{
	Key_Tick();
}
