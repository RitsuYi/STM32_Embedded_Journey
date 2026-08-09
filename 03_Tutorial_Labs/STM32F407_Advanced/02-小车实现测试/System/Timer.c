/*
 * 系统定时调度模块
 *
 * 这个文件把一个硬件定时器配置成 1ms 节拍源，然后基于该节拍
 * 去调度多个不同周期的软件任务，例如：
 * - 编码器测速更新
 * - MPU6050 采样
 * - 电机 PID 控制
 * - 电池电压采样
 * - OLED 刷新
 *
 * 设计上采用“中断只记账，主循环再执行”的思路：
 * - 在中断里只做 very light 的工作：计时、倒计时、挂起任务。
 * - 真正耗时的任务函数在 Timer_RunPendingTasks() 中执行。
 *
 * 这样做可以显著降低中断执行时间，减少中断里做复杂工作的风险。
 */
#include "Timer.h"
#include "BoardConfig.h"
#include "ADC_Bat.h"
#include "Encoder.h"
#include "MPU6050.h"
#include "Motor.h"
#include "OLED.h"

/* 系统运行毫秒计数。 */
static volatile uint32_t g_timerTickMs = 0U;
/* 各个周期任务的倒计时器，单位都是 ms。 */
static volatile uint16_t g_encoderCountdown = BOARD_ENCODER_SAMPLE_MS;
static volatile uint16_t g_mpuCountdown = BOARD_MPU6050_SAMPLE_MS;
static volatile uint16_t g_pidCountdown = BOARD_PID_CONTROL_MS;
static volatile uint16_t g_batCountdown = BOARD_BAT_SAMPLE_MS;
static volatile uint16_t g_oledCountdown = BOARD_OLED_REFRESH_MS;

/* 各任务的“待执行次数”计数器。
 * 使用计数而不是单纯布尔值，意味着如果主循环短时间没来得及处理，
 * 仍然能把漏掉的周期次数补执行回来。 */
static volatile uint8_t g_encoderTaskPending = 0U;
static volatile uint8_t g_mpuTaskPending = 0U;
static volatile uint8_t g_pidTaskPending = 0U;
static volatile uint8_t g_batTaskPending = 0U;
static volatile uint8_t g_oledTaskPending = 0U;

/* 给某个任务增加一次待执行机会。
 * 这里做 255 饱和保护，防止极端情况下计数器溢出回 0。 */
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

	/* 打开定时器时钟。 */
	RCC_APB1PeriphClockCmd(BOARD_TICK_TIMER_RCC, ENABLE);

	/* 上电时先把所有软件时基和待执行计数清零/复位。 */
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

	/* 先把定时器分频到 1MHz，即 1us 计数一次。 */
	prescalerValue = (uint16_t)((BOARD_APB1_TIMER_CLOCK_HZ / 1000000U) - 1U);

	/* 再把自动重装值设成 1000-1，使其每 1000us 触发一次更新中断，即 1ms 节拍。 */
	timeBaseInitStructure.TIM_Prescaler = prescalerValue;
	timeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timeBaseInitStructure.TIM_Period = 1000U - 1U;
	timeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timeBaseInitStructure.TIM_RepetitionCounter = 0U;
	TIM_TimeBaseInit(BOARD_TICK_TIMER, &timeBaseInitStructure);

	/* 清中断标志并使能更新中断。 */
	TIM_ClearITPendingBit(BOARD_TICK_TIMER, TIM_IT_Update);
	TIM_ITConfig(BOARD_TICK_TIMER, TIM_IT_Update, ENABLE);

	/* 配置 NVIC，允许对应定时器中断进入内核。 */
	nvicInitStructure.NVIC_IRQChannel = BOARD_TICK_IRQ;
	nvicInitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
	nvicInitStructure.NVIC_IRQChannelSubPriority = 1U;
	nvicInitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvicInitStructure);

	/* 启动定时器，开始产生 1ms 节拍。 */
	TIM_Cmd(BOARD_TICK_TIMER, ENABLE);
}

uint32_t Timer_GetTick(void)
{
	/* 返回系统累计运行时间，单位 ms。 */
	return g_timerTickMs;
}

void Timer_IRQHandler(void)
{
	if (TIM_GetITStatus(BOARD_TICK_TIMER, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(BOARD_TICK_TIMER, TIM_IT_Update);
		g_timerTickMs++;

		/* 以下所有倒计时都以 1ms 为步进递减。
		 * 减到 0 说明对应周期到达，于是重装倒计时并挂起一次任务。 */
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

	/* 先在关中断条件下，把 ISR 里累计的“待执行次数”整体取出来。
	 * 这样可以避免主循环与中断同时读写这些变量产生竞争。 */
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
	g_oledTaskPending = 0U;//提取完毕后立刻清零，避免重复执行同一轮任务。
	__enable_irq();

	/* 依次把本轮挂起的任务全部执行完。
	 * 之所以使用 while + 局部 pending 计数，是为了在主循环偶发变慢时，
	 * 能把中断期间累计的多次任务补齐，而不是只执行一次。 */
	while ((encoderPending > 0U) || (mpuPending > 0U) || (pidPending > 0U) ||
		   (batPending > 0U) || (oledPending > 0U))
	{
		if (encoderPending > 0U)
		{
			encoderPending--;
			/* 更新编码器增量与速度估计。 */
			Encoder_UpdateSpeed();
		}

		if (mpuPending > 0U)
		{
			mpuPending--;
			/* 进行一次 MPU6050 传感器采样。 */
			MPU6050_SampleTask();
		}

		if (pidPending > 0U)
		{
			pidPending--;
			/* 进行一次电机闭环控制计算。 */
			Motor_ControlTask();
		}

		if (batPending > 0U)
		{
			batPending--;
			/* 进行一次电池电压采样。 */
			ADC_Bat_SampleTask();
		}

		if (oledPending > 0U)
		{
			oledPending--;
			/* 进行一次 OLED 刷新任务。 */
			OLED_Task();
		}
	}
}
