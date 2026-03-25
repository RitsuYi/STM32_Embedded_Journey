#ifndef __WATCH_RTC_H
#define __WATCH_RTC_H

#include "stm32f10x.h"

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
} Time_t;

void RTC_Init(void);
void RTC_Tick1ms(void);
void RTC_AdvanceMilliseconds(uint32_t elapsedMs);
void RTC_SetTime(const Time_t *time);
void RTC_GetTime(Time_t *time);
void RTC_AdjustSeconds(int32_t offsetSeconds);
uint8_t RTC_CalculateWeekday(uint16_t year, uint8_t month, uint8_t day);
uint8_t RTC_IsLeapYear(uint16_t year);

#endif
