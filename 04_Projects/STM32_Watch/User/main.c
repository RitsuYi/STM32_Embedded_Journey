#include "stm32f10x.h"
#include "Button.h"
#include "Encoder.h"
#include "OLED.h"
#include "RTC.h"
#include "Timer.h"

typedef enum
{
    STATE_HOME = 0,
    STATE_MENU,
    STATE_TIMESETTING
} SystemState;

typedef enum
{
    HOME_MENU = 0,
    HOME_SETTINGS = 1
} HomeOption;

#define HOME_OPTION_COUNT 2

static void Watch_FormatDateString(const Time_t *time, char *dateString)
{
    static const char *weekdayText[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

    dateString[0] = (char)('0' + (time->year / 1000U) % 10U);
    dateString[1] = (char)('0' + (time->year / 100U) % 10U);
    dateString[2] = (char)('0' + (time->year / 10U) % 10U);
    dateString[3] = (char)('0' + time->year % 10U);
    dateString[4] = '-';
    dateString[5] = (char)('0' + time->month / 10U);
    dateString[6] = (char)('0' + time->month % 10U);
    dateString[7] = '-';
    dateString[8] = (char)('0' + time->day / 10U);
    dateString[9] = (char)('0' + time->day % 10U);
    dateString[10] = ' ';
    dateString[11] = weekdayText[time->weekday][0];
    dateString[12] = weekdayText[time->weekday][1];
    dateString[13] = weekdayText[time->weekday][2];
    dateString[14] = '\0';
}

static void Watch_FormatTimeString(const Time_t *time, char *timeString)
{
    timeString[0] = (char)('0' + time->hour / 10U);
    timeString[1] = (char)('0' + time->hour % 10U);
    timeString[2] = ':';
    timeString[3] = (char)('0' + time->minute / 10U);
    timeString[4] = (char)('0' + time->minute % 10U);
    timeString[5] = ':';
    timeString[6] = (char)('0' + time->second / 10U);
    timeString[7] = (char)('0' + time->second % 10U);
    timeString[8] = '\0';
}

static void Watch_ShowHomePage(const Time_t *time, HomeOption option)
{
    char dateString[15];
    char timeString[9];

    Watch_FormatDateString(time, dateString);
    Watch_FormatTimeString(time, timeString);

    OLED_ShowString(1, 2, "              ");
    OLED_ShowString(1, 2, dateString);
    OLED_ShowBigString(2, 0, timeString);

    OLED_ShowStringMode(4, 1, " MENU ", (option == HOME_MENU) ? 1U : 0U);
    OLED_ShowString(4, 7, "     ");
    OLED_ShowStringMode(4, 12, " SET ", (option == HOME_SETTINGS) ? 1U : 0U);
}

static void Watch_ShowPlaceholderPage(SystemState state)
{
    if (state == STATE_MENU)
    {
        OLED_ShowString(1, 4, "ENTER MENU");
        OLED_ShowString(2, 3, "PHASE 5 NEXT");
    }
    else
    {
        OLED_ShowString(1, 2, "ENTER SETTING");
        OLED_ShowString(2, 3, "PHASE 6 NEXT");
    }

    OLED_ShowString(3, 1, "                ");
    OLED_ShowString(4, 4, "BTN: BACK");
}

static HomeOption Watch_MoveHomeOption(HomeOption option, int16_t step)
{
    int32_t optionIndex = (int32_t)option + step;

    while (optionIndex < 0)
    {
        optionIndex += HOME_OPTION_COUNT;
    }

    while (optionIndex >= HOME_OPTION_COUNT)
    {
        optionIndex -= HOME_OPTION_COUNT;
    }

    return (HomeOption)optionIndex;
}

static void Watch_RenderScreen(const Time_t *time, SystemState state, HomeOption option, SystemState *lastRenderedState)
{
    if (*lastRenderedState != state)
    {
        OLED_Clear();
        *lastRenderedState = state;
    }

    if (state == STATE_HOME)
    {
        Watch_ShowHomePage(time, option);
    }
    else
    {
        Watch_ShowPlaceholderPage(state);
    }
}

int main(void)
{
    Time_t currentTime;
    uint32_t lastRtcTick = 0;
    uint8_t lastShownSecond = 0xFFU;
    SystemState currentState = STATE_HOME;
    SystemState lastRenderedState = (SystemState)0xFF;
    HomeOption currentOption = HOME_MENU;

    OLED_Init();
    Timer_Init();
    Button_Init();
    Encoder_Init();
    RTC_Init();
    OLED_Clear();

    lastRtcTick = Timer_GetMillis();
    RTC_GetTime(&currentTime);
    Watch_RenderScreen(&currentTime, currentState, currentOption, &lastRenderedState);
    lastShownSecond = currentTime.second;

    while (1)
    {
        uint32_t currentMillis = Timer_GetMillis();
        uint32_t elapsedMs = currentMillis - lastRtcTick;
        int16_t encoderStep = Encoder_Get();
        uint8_t buttonEvent = Button_GetEvent();
        uint8_t refreshRequired = 0U;

        if (elapsedMs != 0U)
        {
            lastRtcTick = currentMillis;
            RTC_AdvanceMilliseconds(elapsedMs);
        }

        if ((currentState == STATE_HOME) && (encoderStep != 0))
        {
            HomeOption updatedOption = Watch_MoveHomeOption(currentOption, encoderStep);

            if (updatedOption != currentOption)
            {
                currentOption = updatedOption;
                refreshRequired = 1U;
            }
        }

        if (buttonEvent != 0U)
        {
            if (currentState == STATE_HOME)
            {
                currentState = (currentOption == HOME_MENU) ? STATE_MENU : STATE_TIMESETTING;
            }
            else
            {
                currentState = STATE_HOME;
            }
            refreshRequired = 1U;
        }

        RTC_GetTime(&currentTime);
        if ((currentState == STATE_HOME) && (currentTime.second != lastShownSecond))
        {
            lastShownSecond = currentTime.second;
            refreshRequired = 1U;
        }
        else
        {
            lastShownSecond = currentTime.second;
        }

        if (refreshRequired != 0U)
        {
            Watch_RenderScreen(&currentTime, currentState, currentOption, &lastRenderedState);
        }
    }
}
