#include "Key.h"
#include "BoardConfig.h"
#include "Timer.h"

#define KEY_RELEASED 0U
#define KEY_PRESSED  1U

static uint8_t g_keyRawState = KEY_RELEASED;
static uint8_t g_keyStableState = KEY_RELEASED;
static uint32_t g_keyDebounceStart = 0U;

static uint8_t Key_ReadRawState(void)
{
	if (GPIO_ReadInputDataBit(BOARD_KEY_GPIO_PORT, BOARD_KEY_PIN) == Bit_SET)
	{
		return KEY_PRESSED;
	}

	return KEY_RELEASED;
}

void Key_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(BOARD_KEY_GPIO_RCC, ENABLE);

	GPIO_InitStructure.GPIO_Pin = BOARD_KEY_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_KEY_GPIO_PORT, &GPIO_InitStructure);

	g_keyRawState = Key_ReadRawState();
	g_keyStableState = g_keyRawState;
	g_keyDebounceStart = Timer_GetTick();
}

uint8_t Key_Scan(void)
{
	uint32_t nowTick;
	uint8_t currentRawState;

	nowTick = Timer_GetTick();
	currentRawState = Key_ReadRawState();

	if (currentRawState != g_keyRawState)
	{
		g_keyRawState = currentRawState;
		g_keyDebounceStart = nowTick;
	}

	if ((nowTick - g_keyDebounceStart) >= BOARD_KEY_DEBOUNCE_MS)
	{
		if (g_keyStableState != g_keyRawState)
		{
			g_keyStableState = g_keyRawState;
			if (g_keyStableState == KEY_PRESSED)
			{
				return 1U;
			}
		}
	}

	return 0U;
}

uint8_t Key_IsPressed(void)
{
	return g_keyStableState;
}
