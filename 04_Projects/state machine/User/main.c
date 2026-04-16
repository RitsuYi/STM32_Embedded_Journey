#include "stm32f10x.h"                  // Device header
#include "Mode1.h"
#include "Mode2.h"
#include "Mode3.h"
#include "Mode4.h"
#include "Global.h"
#include "ModeStore.h"

volatile uint8_t CurrMode = 0, NextMode = 1;

static void Mode_InitById(uint8_t Mode)
{
	switch (Mode)
	{
		case 1: Mode1_Init(); break;
		case 2: Mode2_Init(); break;
		case 3: Mode3_Init(); break;
		case 4: Mode4_Init(); break;
	}
}

static void Mode_ExitById(uint8_t Mode)
{
	switch (Mode)
	{
		case 1: Mode1_Exit(); break;
		case 2: Mode2_Exit(); break;
		case 3: Mode3_Exit(); break;
		case 4: Mode4_Exit(); break;
	}
}

static void Mode_LoopById(uint8_t Mode)
{
	switch (Mode)
	{
		case 1: Mode1_Loop(); break;
		case 2: Mode2_Loop(); break;
		case 3: Mode3_Loop(); break;
		case 4: Mode4_Loop(); break;
	}
}

static void Mode_TickById(uint8_t Mode)
{
	switch (Mode)
	{
		case 1: Mode1_Tick(); break;
		case 2: Mode2_Tick(); break;
		case 3: Mode3_Tick(); break;
		case 4: Mode4_Tick(); break;
	}
}

int main(void)
{
	uint8_t Mode;
	uint8_t TargetMode;
	
	Global_Init();
	
	while (1)
	{
		Global_Loop();
		Mode = CurrMode;
		TargetMode = NextMode;
		
		if (Mode == TargetMode)
		{
			Mode_LoopById(Mode);
		}
		else
		{
			TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
			Mode_ExitById(Mode);
			Mode_InitById(TargetMode);
			CurrMode = TargetMode;
			TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
			TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
			
			ModeStore_Save(TargetMode);
		}
	}
}

void TIM4_IRQHandler(void)
{
	uint8_t Mode;
	
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
	{
		Mode = CurrMode;
		Global_Tick();
		Mode_TickById(Mode);
		
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
