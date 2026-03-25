#include "RTC.h"

static volatile uint32_t RTC_EpochSeconds;
static volatile uint16_t RTC_MillisecondCounter;

static uint8_t RTC_GetMonthDays(uint16_t year, uint8_t month)
{
    static const uint8_t monthDays[12] =
    {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((month == 2U) && RTC_IsLeapYear(year))
    {
        return 29;
    }

    return monthDays[month - 1U];
}

static uint8_t RTC_ParseMonth(const char *monthText)
{
    if ((monthText[0] == 'J') && (monthText[1] == 'a'))
    {
        return 1;
    }
    if ((monthText[0] == 'F') && (monthText[1] == 'e'))
    {
        return 2;
    }
    if ((monthText[0] == 'M') && (monthText[2] == 'r'))
    {
        return 3;
    }
    if ((monthText[0] == 'A') && (monthText[1] == 'p'))
    {
        return 4;
    }
    if ((monthText[0] == 'M') && (monthText[2] == 'y'))
    {
        return 5;
    }
    if ((monthText[0] == 'J') && (monthText[2] == 'n'))
    {
        return 6;
    }
    if ((monthText[0] == 'J') && (monthText[2] == 'l'))
    {
        return 7;
    }
    if ((monthText[0] == 'A') && (monthText[1] == 'u'))
    {
        return 8;
    }
    if ((monthText[0] == 'S') && (monthText[1] == 'e'))
    {
        return 9;
    }
    if ((monthText[0] == 'O') && (monthText[1] == 'c'))
    {
        return 10;
    }
    if ((monthText[0] == 'N') && (monthText[1] == 'o'))
    {
        return 11;
    }

    return 12;
}

static void RTC_LoadBuildTime(Time_t *time)
{
    const char buildDate[] = __DATE__;
    const char buildTime[] = __TIME__;

    time->year = (uint16_t)((buildDate[7] - '0') * 1000 +
                            (buildDate[8] - '0') * 100 +
                            (buildDate[9] - '0') * 10 +
                            (buildDate[10] - '0'));
    time->month = RTC_ParseMonth(buildDate);
    time->day = (uint8_t)(((buildDate[4] == ' ') ? 0 : (buildDate[4] - '0')) * 10 +
                          (buildDate[5] - '0'));
    time->hour = (uint8_t)((buildTime[0] - '0') * 10 + (buildTime[1] - '0'));
    time->minute = (uint8_t)((buildTime[3] - '0') * 10 + (buildTime[4] - '0'));
    time->second = (uint8_t)((buildTime[6] - '0') * 10 + (buildTime[7] - '0'));
    time->weekday = RTC_CalculateWeekday(time->year, time->month, time->day);
}

static uint8_t RTC_IsTimeValid(const Time_t *time)
{
    if (time == 0)
    {
        return 0;
    }

    if ((time->year < 2000U) || (time->month < 1U) || (time->month > 12U))
    {
        return 0;
    }

    if ((time->day < 1U) || (time->day > RTC_GetMonthDays(time->year, time->month)))
    {
        return 0;
    }

    if ((time->hour > 23U) || (time->minute > 59U) || (time->second > 59U))
    {
        return 0;
    }

    return 1;
}

static uint32_t RTC_TimeToEpoch(const Time_t *time)
{
    uint32_t days = 0;
    uint16_t year;
    uint8_t month;

    for (year = 2000U; year < time->year; year++)
    {
        days += RTC_IsLeapYear(year) ? 366U : 365U;
    }

    for (month = 1U; month < time->month; month++)
    {
        days += RTC_GetMonthDays(time->year, month);
    }

    days += (uint32_t)(time->day - 1U);

    return days * 86400U +
           (uint32_t)time->hour * 3600U +
           (uint32_t)time->minute * 60U +
           time->second;
}

static void RTC_EpochToTime(uint32_t epochSeconds, Time_t *time)
{
    uint32_t days = epochSeconds / 86400U;
    uint32_t secondsOfDay = epochSeconds % 86400U;

    time->year = 2000U;
    while (days >= (RTC_IsLeapYear(time->year) ? 366U : 365U))
    {
        days -= RTC_IsLeapYear(time->year) ? 366U : 365U;
        time->year++;
    }

    time->month = 1U;
    while (days >= RTC_GetMonthDays(time->year, time->month))
    {
        days -= RTC_GetMonthDays(time->year, time->month);
        time->month++;
    }

    time->day = (uint8_t)(days + 1U);
    time->hour = (uint8_t)(secondsOfDay / 3600U);
    time->minute = (uint8_t)((secondsOfDay % 3600U) / 60U);
    time->second = (uint8_t)(secondsOfDay % 60U);
    time->weekday = RTC_CalculateWeekday(time->year, time->month, time->day);
}

void RTC_Init(void)
{
    Time_t buildTime;

    RTC_LoadBuildTime(&buildTime);
    RTC_SetTime(&buildTime);
}

void RTC_Tick1ms(void)
{
    RTC_MillisecondCounter++;
    if (RTC_MillisecondCounter >= 1000U)
    {
        RTC_MillisecondCounter = 0;
        RTC_EpochSeconds++;
    }
}

void RTC_SetTime(const Time_t *time)
{
    if (!RTC_IsTimeValid(time))
    {
        return;
    }

    __disable_irq();
    RTC_EpochSeconds = RTC_TimeToEpoch(time);
    RTC_MillisecondCounter = 0;
    __enable_irq();
}

void RTC_GetTime(Time_t *time)
{
    uint32_t epochSeconds;

    if (time == 0)
    {
        return;
    }

    __disable_irq();
    epochSeconds = RTC_EpochSeconds;
    __enable_irq();

    RTC_EpochToTime(epochSeconds, time);
}

void RTC_AdjustSeconds(int32_t offsetSeconds)
{
    int64_t updatedSeconds;

    __disable_irq();
    updatedSeconds = (int64_t)RTC_EpochSeconds + offsetSeconds;
    if (updatedSeconds < 0)
    {
        updatedSeconds = 0;
    }
    RTC_EpochSeconds = (uint32_t)updatedSeconds;
    __enable_irq();
}

uint8_t RTC_CalculateWeekday(uint16_t year, uint8_t month, uint8_t day)
{
    uint16_t adjustedYear = year;
    uint8_t adjustedMonth = month;
    uint16_t century;
    uint16_t yearOfCentury;
    uint8_t zellerValue;

    if (adjustedMonth < 3U)
    {
        adjustedMonth += 12U;
        adjustedYear--;
    }

    century = adjustedYear / 100U;
    yearOfCentury = adjustedYear % 100U;
    zellerValue = (uint8_t)((day +
                             (13U * (adjustedMonth + 1U)) / 5U +
                             yearOfCentury +
                             yearOfCentury / 4U +
                             century / 4U +
                             5U * century) % 7U);

    return (uint8_t)((zellerValue + 6U) % 7U);
}

uint8_t RTC_IsLeapYear(uint16_t year)
{
    if ((year % 400U) == 0U)
    {
        return 1;
    }

    if ((year % 100U) == 0U)
    {
        return 0;
    }

    return ((year % 4U) == 0U) ? 1U : 0U;
}
