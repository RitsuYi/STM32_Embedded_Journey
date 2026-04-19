#include "Timer.h"
#include "BoardConfig.h"
#include "ADC_Bat.h"
#include "Encoder.h"
#include "MPU6050.h"
#include "Motor.h"
#include "OLED.h"

static volatile uint32_t g_timerTickMs = 0U;
static volatile uint16_t g_encoderCountdown = BOARD_ENCODER_SAMPLE_MS;
static volatile uint16_t g_mpuCountdown = BOARD_MPU6050_SAMPLE_MS;
static volatile uint16_t g_pidCountdown = BOARD_PID_CONTROL_MS;
static volatile uint16_t g_batCountdown = BOARD_BAT_SAMPLE_MS;
static volatile uint16_t g_oledCountdown = BOARD_OLED_REFRESH_MS;

static volatile uint8_t g_encoderTaskPending = 0U;
static volatile uint8_t g_mpuTaskPending = 0U;
static volatile uint8_t g_pidTaskPending = 0U;
static volatile uint8_t g_batTaskPending = 0U;
static volatile uint8_t g_oledTaskPending = 0U;

static void Timer_ScheduleTask(volatile uint8_t *pendingCounter)
{
	if (*pendingCounter < 255U)
	{
		(*pendingCounter)++;
	}
}

void Timer_Init(void)
{
	NVIC_InitTypeDef nvicInitStructure;
	TIM_TimeBaseInitTypeDef timeBaseInitStructure;
	uint16_t prescalerValue;

	RCC_APB1PeriphClockCmd(BOARD_TICK_TIMER_RCC, ENABLE);

	g_timerTickMs = 0U;
	g_encoderCountdown = BOARD_ENCODER_SAMPLE_MS;
	g_mpuCountdown = BOARD_MPU6050_SAMPLE_MS;
	g_pidCountdown = BOARD_PID_CONTROL_MS;
	g_batCountdown = BOARD_BAT_SAMPLE_MS;
	g_oledCountdown = BOARD_OLED_REFRESH_MS;

	g_encoderTaskPending = 0U;
	g_mpuTaskPending = 0U;
	g_pidTaskPending = 0U;
	g_batTaskPending = 0U;
	g_oledTaskPending = 0U;

	prescalerValue = (uint16_t)((BOARD_APB1_TIMER_CLOCK_HZ / 1000000U) - 1U);

	timeBaseInitStructure.TIM_Prescaler = prescalerValue;
	timeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timeBaseInitStructure.TIM_Period = 1000U - 1U;
	timeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timeBaseInitStructure.TIM_RepetitionCounter = 0U;
	TIM_TimeBaseInit(BOARD_TICK_TIMER, &timeBaseInitStructure);

	TIM_ClearITPendingBit(BOARD_TICK_TIMER, TIM_IT_Update);
	TIM_ITConfig(BOARD_TICK_TIMER, TIM_IT_Update, ENABLE);

	nvicInitStructure.NVIC_IRQChannel = BOARD_TICK_IRQ;
	nvicInitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
	nvicInitStructure.NVIC_IRQChannelSubPriority = 1U;
	nvicInitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvicInitStructure);

	TIM_Cmd(BOARD_TICK_TIMER, ENABLE);
}

uint32_t Timer_GetTick(void)
{
	return g_timerTickMs;
}

void Timer_IRQHandler(void)
{
	if (TIM_GetITStatus(BOARD_TICK_TIMER, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(BOARD_TICK_TIMER, TIM_IT_Update);
		g_timerTickMs++;

		g_encoderCountdown--;
		if (g_encoderCountdown == 0U)
		{
			g_encoderCountdown = BOARD_ENCODER_SAMPLE_MS;
			Timer_ScheduleTask(&g_encoderTaskPending);
		}

		g_mpuCountdown--;
		if (g_mpuCountdown == 0U)
		{
			g_mpuCountdown = BOARD_MPU6050_SAMPLE_MS;
			Timer_ScheduleTask(&g_mpuTaskPending);
		}

		g_pidCountdown--;
		if (g_pidCountdown == 0U)
		{
			g_pidCountdown = BOARD_PID_CONTROL_MS;
			Timer_ScheduleTask(&g_pidTaskPending);
		}

		g_batCountdown--;
		if (g_batCountdown == 0U)
		{
			g_batCountdown = BOARD_BAT_SAMPLE_MS;
			Timer_ScheduleTask(&g_batTaskPending);
		}

		g_oledCountdown--;
		if (g_oledCountdown == 0U)
		{
			g_oledCountdown = BOARD_OLED_REFRESH_MS;
			Timer_ScheduleTask(&g_oledTaskPending);
		}
	}
}

void Timer_RunPendingTasks(void)
{
	uint8_t encoderPending;
	uint8_t mpuPending;
	uint8_t pidPending;
	uint8_t batPending;
	uint8_t oledPending;

	__disable_irq();
	encoderPending = g_encoderTaskPending;
	mpuPending = g_mpuTaskPending;
	pidPending = g_pidTaskPending;
	batPending = g_batTaskPending;
	oledPending = g_oledTaskPending;

	g_encoderTaskPending = 0U;
	g_mpuTaskPending = 0U;
	g_pidTaskPending = 0U;
	g_batTaskPending = 0U;
	g_oledTaskPending = 0U;
	__enable_irq();

	while ((encoderPending > 0U) || (mpuPending > 0U) || (pidPending > 0U) ||
		   (batPending > 0U) || (oledPending > 0U))
	{
		if (encoderPending > 0U)
		{
			encoderPending--;
			Encoder_UpdateSpeed();
		}

		if (mpuPending > 0U)
		{
			mpuPending--;
			MPU6050_SampleTask();
		}

		if (pidPending > 0U)
		{
			pidPending--;
			Motor_ControlTask();
		}

		if (batPending > 0U)
		{
			batPending--;
			ADC_Bat_SampleTask();
		}

		if (oledPending > 0U)
		{
			oledPending--;
			OLED_Task();
		}
	}
}
