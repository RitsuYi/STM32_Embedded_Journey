#include "stm32f10x.h"
#include "Buzzer.h"

typedef struct
{
	uint16_t frequency;
	uint16_t durationMs;
} BuzzerNote_t;

static const BuzzerNote_t SuccessNotes[] =
{
	{660, 100},
	{880, 100},
	{1047, 160},
	{0, 0}
};

static const BuzzerNote_t AlarmNotes[] =
{
	{900, 180},
	{0, 100},
	{900, 180},
	{0, 100},
	{700, 260},
	{0, 0}
};

static const BuzzerNote_t *BuzzerSequence;
static uint8_t BuzzerIndex;
static uint8_t BuzzerActive;
static uint32_t BuzzerNextTime;

static void Buzzer_SetFrequency(uint16_t frequency)
{
	uint32_t period;
	
	if (frequency == 0)
	{
		TIM_SetCompare1(TIM3, 0);
		TIM_Cmd(TIM3, DISABLE);
		GPIO_SetBits(GPIOA, GPIO_Pin_6);
		return;
	}
	
	period = 1000000UL / frequency;
	if (period < 2)
	{
		period = 2;
	}
	
	TIM_SetAutoreload(TIM3, (uint16_t)(period - 1));
	TIM_SetCompare1(TIM3, (uint16_t)(period / 2));
	TIM_GenerateEvent(TIM3, TIM_EventSource_Update);
	TIM_Cmd(TIM3, ENABLE);
}

static void Buzzer_PlaySequence(const BuzzerNote_t *sequence)
{
	BuzzerSequence = sequence;
	BuzzerIndex = 0;
	BuzzerActive = 1;
	BuzzerNextTime = 0;
}

void Buzzer_Init(void)
{
	uint16_t prescaler;
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	prescaler = (uint16_t)((SystemCoreClock / 1000000UL) - 1);
	TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
	TIM_TimeBaseStructure.TIM_Prescaler = prescaler;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(TIM3, ENABLE);
	
	BuzzerSequence = 0;
	BuzzerIndex = 0;
	BuzzerActive = 0;
	BuzzerNextTime = 0;
	Buzzer_Stop();
}

void Buzzer_Stop(void)
{
	BuzzerActive = 0;
	BuzzerSequence = 0;
	BuzzerIndex = 0;
	BuzzerNextTime = 0;
	Buzzer_SetFrequency(0);
}

void Buzzer_PlaySuccess(void)
{
	Buzzer_PlaySequence(SuccessNotes);
}

void Buzzer_PlayAlarm(void)
{
	Buzzer_PlaySequence(AlarmNotes);
}

void Buzzer_Update(uint32_t nowMs)
{
	const BuzzerNote_t *note;
	
	if (!BuzzerActive || BuzzerSequence == 0)
	{
		return;
	}
	
	if (BuzzerNextTime != 0 && (uint32_t)(nowMs - BuzzerNextTime) > 0x7FFFFFFFUL)
	{
		return;
	}
	
	note = &BuzzerSequence[BuzzerIndex];
	if (note->durationMs == 0)
	{
		Buzzer_Stop();
		return;
	}
	
	Buzzer_SetFrequency(note->frequency);
	BuzzerNextTime = nowMs + note->durationMs;
	BuzzerIndex++;
}
