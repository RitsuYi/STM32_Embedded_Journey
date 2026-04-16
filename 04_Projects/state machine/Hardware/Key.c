#include "stm32f10x.h"                  // Device header
#include "Key.h"

#define KEY_PRESSED				1
#define KEY_UNPRESSED			0

#define KEY_TIME_DOUBLE			200
#define KEY_TIME_LONG			2000
#define KEY_TIME_REPEAT			100

static volatile uint8_t Key_Flag[KEY_COUNT];

void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;//主按键，接在PA11
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
	
	/*
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;//备用按键，接在PB2
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	*/
}

uint8_t Key_GetState(uint8_t n)
{
	if (n == KEY_1)
	{
		return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == 0 ? KEY_PRESSED : KEY_UNPRESSED;//三目运算符 “条件 ? 条件成立时的值 : 条件不成立时的值”
	}
	
	/*
	else if (n == KEY_2)
	{
		return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2) == 0 ? KEY_PRESSED : KEY_UNPRESSED;//备用按键，上面是主按键
	}
	*/
	
	return KEY_UNPRESSED;
}

uint8_t Key_Check(uint8_t n, uint8_t Flag)
{
	uint32_t Primask;
	uint8_t Result = 0;
	
	if (n >= KEY_COUNT)
	{
		return 0;
	}
	
	Primask = __get_PRIMASK();
	__disable_irq();
	
	if ((Key_Flag[n] & Flag) != 0)
	{
		Key_Flag[n] &= ~Flag;
		Result = 1;
	}
	
	if (Primask == 0)
	{
		__enable_irq();
	}
	
	return Result;
}

void Key_Tick(void)
{
	static uint8_t Count, i;
	static uint8_t CurrState[KEY_COUNT], PrevState[KEY_COUNT];//新与旧
	static uint8_t S[KEY_COUNT];
	static uint16_t Time[KEY_COUNT];

	for (i = 0; i < KEY_COUNT; i++)
	{
		if (Time[i] > 0)
		{
			Time[i] --;
		}
	}

	Count ++;
	if (Count >= 20)
	{
		Count = 0;

		for (i = 0; i < KEY_COUNT; i++)
		{
			PrevState[i] = CurrState[i];
			CurrState[i] = Key_GetState(i);
			
			if (CurrState[i] == KEY_PRESSED)
			{
				Key_Flag[i] |= KEY_HOLD;
			}
			else
			{
				Key_Flag[i] &= ~KEY_HOLD;
			}
			
			switch(S[i])
			{
				case 0:
					if (CurrState[i] == KEY_PRESSED && PrevState[i] == KEY_UNPRESSED)
					{
						Key_Flag[i] |= KEY_DOWN;
						Time[i] = KEY_TIME_LONG;
						S[i] = 1;
					}
					break;
				
				case 1:
					if (CurrState[i] == KEY_UNPRESSED && PrevState[i] == KEY_PRESSED)
					{
						Key_Flag[i] |= KEY_UP;
						Time[i] = KEY_TIME_DOUBLE;
						S[i] = 2;
					}
					else if (Time[i] == 0)
					{
						Key_Flag[i] |= KEY_LONG;
						Time[i] = KEY_TIME_REPEAT;
						S[i] = 4;
					}
					break;

				case 2:
					if (CurrState[i] == KEY_PRESSED && PrevState[i] == KEY_UNPRESSED)
					{
						Key_Flag[i] |= KEY_DOUBLE;
						S[i] = 3;
					}
					else if (Time[i] == 0)
					{
						Key_Flag[i] |= KEY_SINGLE;
						S[i] = 0;
					}
					break;

				case 3:
					if (CurrState[i] == KEY_UNPRESSED && PrevState[i] == KEY_PRESSED)
					{
						S[i] = 0;
					}
					break;

				case 4:
					if (CurrState[i] == KEY_UNPRESSED && PrevState[i] == KEY_PRESSED)
					{
						S[i] = 0;
					}
					else if (Time[i] == 0)
					{
						Key_Flag[i] |= KEY_REPEAT;
						Time[i] = KEY_TIME_REPEAT;
					}
					break;				
			}
		}
	}
}
