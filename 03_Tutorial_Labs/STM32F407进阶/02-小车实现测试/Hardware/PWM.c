/*
 * PWM 输出模块
 *
 * 这个文件负责把一个定时器初始化为 4 路 PWM 输出，
 * 并提供“按占空比设置输出”的统一接口，供电机驱动层使用。
 *
 * 这里的占空比不是直接暴露底层 CCR 比较值，而是使用
 * 0 ~ BOARD_PWM_MAX_DUTY 的逻辑范围，方便上层做统一控制与限幅。
 */
#include "PWM.h"
#include "BoardConfig.h"

/* 底层比较寄存器写入函数。
 * 不同通道对应 TIM 的不同 CCR 寄存器，这里统一封装后，上层只需要关心逻辑通道号。 */
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

	/* 打开 GPIO 与定时器时钟。 */
	RCC_AHB1PeriphClockCmd(BOARD_PWM_GPIO_RCC, ENABLE);
	RCC_APB1PeriphClockCmd(BOARD_PWM_TIMER_RCC, ENABLE);

	/* 将 4 个 PWM 引脚切到对应定时器复用功能。 */
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_A_PINSOURCE, BOARD_PWM_GPIO_AF);
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_B_PINSOURCE, BOARD_PWM_GPIO_AF);
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_C_PINSOURCE, BOARD_PWM_GPIO_AF);
	GPIO_PinAFConfig(BOARD_PWM_GPIO_PORT, BOARD_PWM_D_PINSOURCE, BOARD_PWM_GPIO_AF);

	/* 四路输出脚统一配置为复用推挽输出。 */
	GPIO_InitStructure.GPIO_Pin = BOARD_PWM_A_PIN | BOARD_PWM_B_PIN | BOARD_PWM_C_PIN | BOARD_PWM_D_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(BOARD_PWM_GPIO_PORT, &GPIO_InitStructure);

	/* 配置 PWM 基本频率：
	 * PWM 频率 = 定时器时钟 / (PSC + 1) / (ARR + 1)。 */
	TIM_TimeBaseStructure.TIM_Prescaler = BOARD_PWM_PRESCALER;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = BOARD_PWM_PERIOD;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0U;
	TIM_TimeBaseInit(BOARD_PWM_TIMER, &TIM_TimeBaseStructure);

	/* 配置 4 路通道为 PWM1 模式，初始脉宽为 0，即默认无输出。 */
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0U;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	TIM_OC1Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);
	TIM_OC2Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);
	TIM_OC3Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);
	TIM_OC4Init(BOARD_PWM_TIMER, &TIM_OCInitStructure);

	/* 使能预装载，避免更新比较值时产生毛刺。 */
	TIM_OC1PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(BOARD_PWM_TIMER, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(BOARD_PWM_TIMER, ENABLE);

	/* 启动定时器后，统一拉到 0 占空比，保证上电安全。 */
	TIM_Cmd(BOARD_PWM_TIMER, ENABLE);
	PWM_StopAll();
}

void PWM_SetDuty(PWM_Channel_t channel, uint16_t duty)
{
	uint32_t compareValue;

	/* 占空比做上限保护，防止越界写入。 */
	if (duty > BOARD_PWM_MAX_DUTY)
	{
		duty = BOARD_PWM_MAX_DUTY;
	}

	/* 将逻辑占空比区间映射到底层 CCR 比较值区间。 */
	compareValue = ((uint32_t)(BOARD_PWM_PERIOD + 1U) * duty) / BOARD_PWM_MAX_DUTY;
	PWM_SetCompare(channel, (uint16_t)compareValue);
}

void PWM_StopAll(void)
{
	/* 四路通道全部置零，用于紧急停机或初始化安全态。 */
	PWM_SetDuty(PWM_CHANNEL_A, 0U);
	PWM_SetDuty(PWM_CHANNEL_B, 0U);
	PWM_SetDuty(PWM_CHANNEL_C, 0U);
	PWM_SetDuty(PWM_CHANNEL_D, 0U);
}

uint16_t PWM_GetMaxDuty(void)
{
	/* 向上层暴露统一的占空比上限，便于做 PID 输出限幅。 */
	return BOARD_PWM_MAX_DUTY;
}
