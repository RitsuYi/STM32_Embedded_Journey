#ifndef __RTC_H__
#define __RTC_H__

#include "stm32f10x.h"
#include <stdint.h>

// 时间结构体
typedef struct {
	uint8_t year;    // 年（后两位，如26表示2026年）
	uint8_t month;   // 月 1-12
	uint8_t day;     // 日 1-31
	uint8_t hour;    // 时 0-23
	uint8_t minute;  // 分 0-59
	uint8_t second;  // 秒 0-59
	uint8_t weekday; // 星期 0-6 (周日=0, 周一=1, ..., 周六=6)
} Time_t;

// 秒表结构体
typedef struct {
	uint8_t running;
	uint16_t minutes;
	uint8_t seconds;
	uint16_t milliseconds;
} Stopwatch_t;

// RTC初始化
void RTC_Init(void);

// 时间获取和设置
void RTC_GetTime(Time_t *time);
void RTC_SetTime(Time_t *time);

// 时间更新（每秒调用一次）
void RTC_Update(void);

// 星期计算（基姆拉尔森公式）
uint8_t RTC_CalculateWeekday(uint8_t year, uint8_t month, uint8_t day);

// 秒表功能
void Stopwatch_Init(void);
void Stopwatch_Start(void);
void Stopwatch_Stop(void);
void Stopwatch_Reset(void);
void Stopwatch_GetTime(Stopwatch_t *sw);
void Stopwatch_Update(void);  // 每10ms调用一次

#endif
