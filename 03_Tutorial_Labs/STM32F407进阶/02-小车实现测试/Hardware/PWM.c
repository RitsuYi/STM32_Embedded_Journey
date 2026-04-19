#include "PWM.h"
#include "BoardConfig.h"

static void PWM_SetCompare(PWM_Channel_t channel, uint16_t compare)
{
	switch (channel)
	{
		case PWM_CHANNEL_A:
			TIM_SetCompare1(BOARD_PWM_TIMER, compare);
			break;

		case PWM_CHANNEL_B:
			TIM_SetCompare2(BOARD_PWM_TIMER, compare);
			break;

		case PWM_CHANNEL_C:
			TIM_SetCompare3(BOARD_PWM_TIMER, compare);
			break;

		case PWM_CHANNEL_D:
			TIM_SetCompare4(BOARD_PWM_TIMER, compare);
			break;

		default:
			break;
	}
}

void PWM_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	RCC_AHB1PeriphClockCmd(BOARD_PWM_GPIO_RCC, ENABLE);
	RCC_APB1PeriphClockCmd(BOARD_PWM_TIMER_RCC, ENABLE);

	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_A_PINSOURCE, BOARD_PWM_GPIO_AF);
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_B_PINSOURCE, BOARD_PWM_GPIO_AF);
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_C_PINSOURCE, BOARD_PWM_GPIO_AF);
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_D_PINSOURCE, BOARD_PWM_GPIO_AF);

	GPIO_InitStructure.GPIO_Pin = BOARD_PWM_A_PIN | BOARD_PWM_B_PIN | BOARD_PWM_C_PIN | BOARD_PWM_D_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(BOARD_PWM_GPIO_PORT, &GPIO_InitStructure);

	TIM_TimeBaseStructure.TIM_Prescaler = BOARD_PWM_PRESCALER;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = BOARD_PWM_PERIOD;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0U;
	TIM_TimeBaseInit(BOARD_PWM_TIMER, &TIM_TimeBaseStructure);

	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0U;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	TIM_OC1Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);
	TIM_OC2Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);
	TIM_OC3Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);
	TIM_OC4Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);

	TIM_OC1PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(BOARD_PWM_TIMER, ENABLE);

	TIM_Cmd(BOARD_PWM_TIMER, ENABLE);
	PWM_StopAll();
}

void PWM_SetDuty(PWM_Channel_t channel, uint16_t duty)
{
	uint32_t compareValue;

	if (duty > BOARD_PWM_MAX_DUTY)
	{
		duty = BOARD_PWM_MAX_DUTY;
	}

	compareValue = ((uint32_t)(BOARD_PWM_PERIOD + 1U) * duty) / BOARD_PWM_MAX_DUTY;
	PWM_SetCompare(channel, (uint16_t)compareValue);
}

void PWM_StopAll(void)
{
	PWM_SetDuty(PWM_CHANNEL_A, 0U);
	PWM_SetDuty(PWM_CHANNEL_B, 0U);
	PWM_SetDuty(PWM_CHANNEL_C, 0U);
	PWM_SetDuty(PWM_CHANNEL_D, 0U);
}

uint16_t PWM_GetMaxDuty(void)
{
	return BOARD_PWM_MAX_DUTY;
}
