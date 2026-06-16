#include "stm32f10x.h"
#include "Key.h"

#define KEY_COUNT             6
#define KEY_DEBOUNCE_TICKS    2

static const uint16_t KeyPins[KEY_COUNT] =
{
	GPIO_Pin_0,
	GPIO_Pin_1,
	GPIO_Pin_2,
	GPIO_Pin_3,
	GPIO_Pin_4,
	GPIO_Pin_5
};

static uint8_t KeyRawState[KEY_COUNT];
static uint8_t KeyStableState[KEY_COUNT];
static uint8_t KeyStableTicks[KEY_COUNT];
static uint8_t KeyPendingEvents;

void Key_Init(void)
{
	uint8_t i;
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
	                              GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	for (i = 0; i < KEY_COUNT; i++)
	{
		KeyRawState[i] = 0;
		KeyStableState[i] = 0;
		KeyStableTicks[i] = 0;
	}
	KeyPendingEvents = 0;
}

void Key_Update(void)
{
	uint8_t i;
	uint8_t currentState;
	
	for (i = 0; i < KEY_COUNT; i++)
	{
		currentState = (GPIO_ReadInputDataBit(GPIOA, KeyPins[i]) == Bit_RESET) ? 1 : 0;
		
		if (currentState != KeyRawState[i])
		{
			KeyRawState[i] = currentState;
			KeyStableTicks[i] = 0;
		}
		else if (KeyStableTicks[i] < KEY_DEBOUNCE_TICKS)
		{
			KeyStableTicks[i]++;
		}
		else if (currentState != KeyStableState[i])
		{
			KeyStableState[i] = currentState;
			if (currentState)
			{
				KeyPendingEvents |= (uint8_t)(1 << i);
			}
		}
	}
}

uint8_t Key_GetEvents(void)
{
	uint8_t events;
	
	events = KeyPendingEvents;
	KeyPendingEvents = 0;
	
	return events;
}

uint8_t Key_IsPressed(uint8_t keyIndex)
{
	if (keyIndex >= KEY_COUNT)
	{
		return 0;
	}
	
	return KeyStableState[keyIndex];
}

