#include "stm32f10x.h"
#include "LED.h"

static uint16_t LED_GetTeamPin(uint8_t team)
{
	switch (team)
	{
		case 1: return GPIO_Pin_12;
		case 2: return GPIO_Pin_13;
		case 3: return GPIO_Pin_14;
		case 4: return GPIO_Pin_15;
		default: return 0;
	}
}

void LED_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
	                       RCC_APB2Periph_GPIOB |
	                       RCC_APB2Periph_GPIOC, ENABLE);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	LED_AllOff();
}

void LED_AllOff(void)
{
	LED_RunOff();
	LED_TeamAllOff();
	LED_AlarmOff();
}

void LED_RunOn(void)
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}

void LED_RunOff(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

void LED_RunToggle(void)
{
	if (GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET)
	{
		LED_RunOff();
	}
	else
	{
		LED_RunOn();
	}
}

void LED_TeamAllOff(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
}

void LED_TeamOn(uint8_t team)
{
	uint16_t pin;
	
	pin = LED_GetTeamPin(team);
	if (pin != 0)
	{
		GPIO_SetBits(GPIOB, pin);
	}
}

void LED_SetWinner(uint8_t team)
{
	LED_TeamAllOff();
	LED_TeamOn(team);
}

void LED_AlarmOn(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

void LED_AlarmOff(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_8);
}

void LED_AlarmToggle(void)
{
	if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_8) == Bit_RESET)
	{
		LED_AlarmOn();
	}
	else
	{
		LED_AlarmOff();
	}
}

