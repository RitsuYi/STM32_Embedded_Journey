#include "stm32f10x.h"
#include "RTC.h"

// 全局时间变量
static Time_t g_CurrentTime = {
	26,   // year: 2026
	3,    // month: 3月
	24,   // day: 24日
	12,   // hour: 12时
	30,   // minute: 30分
	0,    // second: 0秒
	2     // weekday: 周二
};

// 秒表变量
static Stopwatch_t g_Stopwatch = {0, 0, 0, 0};

// 每月天数表（非闰年）
static const uint8_t g_DaysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
  * 函    数：RTC初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用TIM2定时器实现软件RTC
  */
void RTC_Init(void)
{
	// 开启TIM2时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	// 配置TIM2为1s定时器
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStruct.TIM_Period = 10000 - 1;         // ARR = 10000-1
	TIM_TimeBaseInitStruct.TIM_Prescaler = 7200 - 1;      // PSC = 7200-1
	                                                      // 72MHz / 7200 = 10kHz
	                                                      // 10kHz / 10000 = 1Hz (1秒)
	TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);

	// 开启TIM2更新中断
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	// 开启TIM2
	TIM_Cmd(TIM2, ENABLE);

	// 开启NVIC中断
	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStruct);

	// 计算初始星期
	g_CurrentTime.weekday = RTC_CalculateWeekday(g_CurrentTime.year, g_CurrentTime.month, g_CurrentTime.day);
}

/**
  * 函    数：获取当前时间
  * 参    数：time - 时间结构体指针
  * 返 回 值：无
  */
void RTC_GetTime(Time_t *time)
{
	if (time != NULL)
	{
		*time = g_CurrentTime;
	}
}

/**
  * 函    数：设置时间
  * 参    数：time - 时间结构体指针
  * 返 回 值：无
  */
void RTC_SetTime(Time_t *time)
{
	if (time != NULL)
	{
		g_CurrentTime = *time;
		// 重新计算星期
		g_CurrentTime.weekday = RTC_CalculateWeekday(g_CurrentTime.year, g_CurrentTime.month, g_CurrentTime.day);
	}
}

/**
  * 函    数：时间更新（每秒调用一次）
  * 参    数：无
  * 返 回 值：无
  * 说    明：由TIM2中断调用，更新系统时间
  */
void RTC_Update(void)
{
	g_CurrentTime.second++;

	// 秒进位
	if (g_CurrentTime.second >= 60)
	{
		g_CurrentTime.second = 0;
		g_CurrentTime.minute++;

		// 分进位
		if (g_CurrentTime.minute >= 60)
		{
			g_CurrentTime.minute = 0;
			g_CurrentTime.hour++;

			// 时进位
			if (g_CurrentTime.hour >= 24)
			{
				g_CurrentTime.hour = 0;
				g_CurrentTime.day++;
				g_CurrentTime.weekday++;  // 星期加1

				// 星期循环
				if (g_CurrentTime.weekday >= 7)
				{
					g_CurrentTime.weekday = 0;  // 周日
				}

				// 日进位
				uint8_t days = g_DaysInMonth[g_CurrentTime.month];

				// 闰年2月判断
				if (g_CurrentTime.month == 2)
				{
					// 判断闰年：能被4整除但不能被100整除，或能被400整除
					uint16_t full_year = 2000 + g_CurrentTime.year;
					if ((full_year % 4 == 0 && full_year % 100 != 0) || (full_year % 400 == 0))
					{
						days = 29;  // 闰年2月29天
					}
				}

				if (g_CurrentTime.day > days)
				{
					g_CurrentTime.day = 1;
					g_CurrentTime.month++;

					// 月进位
					if (g_CurrentTime.month > 12)
					{
						g_CurrentTime.month = 1;
						g_CurrentTime.year++;  // 年进位
					}
				}
			}
		}
	}
}

/**
  * 函    数：计算星期几（基姆拉尔森公式）
  * 参    数：year - 年（后两位）
           month - 月 (1-12)
           day - 日 (1-31)
  * 返 回 值：0-6 (0=周日, 1=周一, ..., 6=周六)
  * 说    明：基姆拉尔森公式：W = (d + 2m + 3(m+1)/5 + y + y/4 - y/100 + y/400 + 1) % 7
           注意：该公式中1月和2月要当作上一年的13月和14月计算
  */
uint8_t RTC_CalculateWeekday(uint8_t year, uint8_t month, uint8_t day)
{
	int y, m, d;
	int w;

	// 基姆拉尔森公式：1月、2月当作上一年的13、14月
	if (month == 1 || month == 2)
	{
		y = 2000 + year - 1;
		m = month + 12;
	}
	else
	{
		y = 2000 + year;
		m = month;
	}
	d = day;

	// 基姆拉尔森公式
	w = (d + 2 * m + 3 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400 + 1) % 7;

	return w;  // 0=周日, 1=周一, ..., 6=周六
}

/**
  * 函    数：秒表初始化
  * 参    数：无
  * 返 回 值：无
  */
void Stopwatch_Init(void)
{
	g_Stopwatch.running = 0;
	g_Stopwatch.minutes = 0;
	g_Stopwatch.seconds = 0;
	g_Stopwatch.milliseconds = 0;
}

/**
  * 函    数：秒表启动
  * 参    数：无
  * 返 回 值：无
  */
void Stopwatch_Start(void)
{
	g_Stopwatch.running = 1;
}

/**
  * 函    数：秒表停止
  * 参    数：无
  * 返 回 值：无
  */
void Stopwatch_Stop(void)
{
	g_Stopwatch.running = 0;
}

/**
  * 函    数：秒表复位
  * 参    数：无
  * 返 回 值：无
  */
void Stopwatch_Reset(void)
{
	g_Stopwatch.running = 0;
	g_Stopwatch.minutes = 0;
	g_Stopwatch.seconds = 0;
	g_Stopwatch.milliseconds = 0;
}

/**
  * 函    数：获取秒表时间
  * 参    数：sw - 秒表结构体指针
  * 返 回 值：无
  */
void Stopwatch_GetTime(Stopwatch_t *sw)
{
	if (sw != NULL)
	{
		*sw = g_Stopwatch;
	}
}

/**
  * 函    数：秒表更新（每10ms调用一次）
  * 参    数：无
  * 返 回 值：无
  * 说    明：需要在主循环中每10ms调用一次
  */
void Stopwatch_Update(void)
{
	if (g_Stopwatch.running)
	{
		g_Stopwatch.milliseconds++;

		// 毫秒进位（100ms进1秒）
		if (g_Stopwatch.milliseconds >= 100)
		{
			g_Stopwatch.milliseconds = 0;
			g_Stopwatch.seconds++;

			// 秒进位
			if (g_Stopwatch.seconds >= 60)
			{
				g_Stopwatch.seconds = 0;
				g_Stopwatch.minutes++;

				// 分进位（最大59分59秒）
				if (g_Stopwatch.minutes >= 60)
				{
					g_Stopwatch.minutes = 59;
					g_Stopwatch.seconds = 59;
				}
			}
		}
	}
}
