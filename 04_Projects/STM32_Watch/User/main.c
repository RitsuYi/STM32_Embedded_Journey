#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"
#include "Encoder.h"
#include "Button.h"
#include "LED.h"
#include "RTC.h"
#include <stdio.h>

int16_t Encoder_Count;
Time_t g_Time;

// 星期数组
static const char *g_WeekDay[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

int main(void)
{
	OLED_Init();
	Timer_Init();
	Encoder_Init();
	Button_Init();
	LED_Init();
	RTC_Init();

	OLED_Clear();

	while (1)
	{
		// 获取编码器增量
		Encoder_Count = Encoder_Get();

		// 获取当前时间
		RTC_GetTime(&g_Time);

		// 显示日期和星期
		OLED_ShowString(0, 0, "20");
		OLED_ShowNum(16, 0, g_Time.year, 2);
		OLED_ShowChar(32, 0, '.');
		OLED_ShowNum(40, 0, g_Time.month, 2);
		OLED_ShowChar(56, 0, '.');
		OLED_ShowNum(64, 0, g_Time.day, 2);
		OLED_ShowString(80, 0, " ");

		// 显示星期
		OLED_ShowString(88, 0, (char*)g_WeekDay[g_Time.weekday]);

		// 显示时间（大字体）
		OLED_ShowNum(16, 2, g_Time.hour, 2);
		OLED_ShowChar(32, 2, ':');
		OLED_ShowNum(40, 2, g_Time.minute, 2);
		OLED_ShowChar(56, 2, ':');
		OLED_ShowNum(64, 2, g_Time.second, 2);

		// 显示编码器数值（测试用）
		OLED_ShowString(0, 4, "Enc:");
		OLED_ShowSignedNum(24, 4, Encoder_Count, 5);

		// 检测按键
		if (Button_Pressed())
		{
			LED1_Turn();  // 按键按下，切换LED1状态
		}

		// 延时避免刷新过快
		Delay_ms(50);
	}
}
