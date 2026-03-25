#include "Menu.h"
#include "LED.h"
#include "OLED.h"

#define MENU_STOPWATCH_MAX_MS      5999990UL
#define MENU_STOPWATCH_REFRESH_MS  20UL
#define MENU_ANIMATION_UNIT        256
#define MENU_ANIMATION_RING        (MENU_OPTION_COUNT * MENU_ANIMATION_UNIT)
#define MENU_ANIMATION_SPEED       1536U
#define MENU_ANIMATION_FRAME_MS    16U
#define MENU_CAROUSEL_CENTER_X     64
#define MENU_CAROUSEL_SPACING      56
#define MENU_ICON_BOTTOM_Y         38
#define MENU_LABEL_TOP_Y           41
#define MENU_ICON_SIDE_SCALE       75U
#define MENU_ICON_CENTER_SCALE     150U
#define MENU_TEXT_SIDE_SCALE       72U
#define MENU_TEXT_CENTER_SCALE     110U

static uint8_t Menu_NormalizeIndex(int32_t value, uint8_t optionCount)
{
    while (value < 0)
    {
        value += optionCount;
    }

    while (value >= optionCount)
    {
        value -= optionCount;
    }

    return (uint8_t)value;
}

static uint8_t Menu_MoveSelection(uint8_t currentValue, uint8_t optionCount, int16_t step)
{
    return Menu_NormalizeIndex((int32_t)currentValue + step, optionCount);
}

static uint8_t Menu_AdjustWrappedValue(uint8_t currentValue, uint8_t minimumValue, uint8_t maximumValue, int16_t step)
{
    int32_t normalizedValue = (int32_t)currentValue - minimumValue + step;
    int32_t range = (int32_t)maximumValue - minimumValue + 1;

    while (normalizedValue < 0)
    {
        normalizedValue += range;
    }

    while (normalizedValue >= range)
    {
        normalizedValue -= range;
    }

    return (uint8_t)(normalizedValue + minimumValue);
}

static uint16_t Menu_AdjustWrappedYear(uint16_t currentValue, uint16_t minimumValue, uint16_t maximumValue, int16_t step)
{
    int32_t normalizedValue = (int32_t)currentValue - minimumValue + step;
    int32_t range = (int32_t)maximumValue - minimumValue + 1;

    while (normalizedValue < 0)
    {
        normalizedValue += range;
    }

    while (normalizedValue >= range)
    {
        normalizedValue -= range;
    }

    return (uint16_t)(normalizedValue + minimumValue);
}

static int16_t Menu_AbsInt16(int16_t value)
{
    return (value < 0) ? (int16_t)(-value) : value;
}

static uint8_t Menu_ScaleValue(uint8_t baseValue, uint8_t scalePercent)
{
    uint16_t scaledValue = ((uint16_t)baseValue * scalePercent + 99U) / 100U;

    if (scaledValue == 0U)
    {
        scaledValue = 1U;
    }

    return (uint8_t)scaledValue;
}

static int16_t Menu_GetWrappedMenuPosition(int32_t position)
{
    while (position < 0)
    {
        position += MENU_ANIMATION_RING;
    }

    while (position >= MENU_ANIMATION_RING)
    {
        position -= MENU_ANIMATION_RING;
    }

    return position;
}

static int16_t Menu_GetMenuRelativePosition(uint8_t optionIndex, int32_t visualPosition)
{
    int16_t normalizedVisual = Menu_GetWrappedMenuPosition(visualPosition);
    int16_t delta = (int16_t)(optionIndex * MENU_ANIMATION_UNIT) - normalizedVisual;

    while (delta > (MENU_ANIMATION_RING / 2))
    {
        delta -= MENU_ANIMATION_RING;
    }

    while (delta < -(MENU_ANIMATION_RING / 2))
    {
        delta += MENU_ANIMATION_RING;
    }

    return delta;
}

static uint8_t Menu_GetFocusScale(uint8_t sideScale, uint8_t centerScale, int16_t distance)
{
    uint16_t focusStrength;

    if (distance >= MENU_ANIMATION_UNIT)
    {
        return sideScale;
    }

    focusStrength = (uint16_t)(MENU_ANIMATION_UNIT - distance);
    return (uint8_t)(sideScale + ((uint16_t)(centerScale - sideScale) * focusStrength) / MENU_ANIMATION_UNIT);
}

static void Menu_SyncMenuAnimation(MenuContext *context)
{
    int32_t snappedPosition = (int32_t)context->menuOption * MENU_ANIMATION_UNIT;

    context->menuVisualPosition = snappedPosition;
    context->menuTargetPosition = snappedPosition;
    context->menuAnimationAccumulatorMs = 0U;
}

static void Menu_UpdateMenuAnimation(MenuContext *context, uint32_t elapsedMs)
{
    int32_t delta;
    int32_t step;

    if (context->currentState != STATE_MENU)
    {
        context->menuAnimationAccumulatorMs = 0U;
        return;
    }

    delta = (int32_t)context->menuTargetPosition - context->menuVisualPosition;
    if (delta == 0)
    {
        context->menuAnimationAccumulatorMs = 0U;
        return;
    }

    context->menuAnimationAccumulatorMs = (uint16_t)(context->menuAnimationAccumulatorMs + elapsedMs);
    if (context->menuAnimationAccumulatorMs < MENU_ANIMATION_FRAME_MS)
    {
        return;
    }

    step = ((uint32_t)context->menuAnimationAccumulatorMs * MENU_ANIMATION_SPEED + 999U) / 1000U;
    context->menuAnimationAccumulatorMs = 0U;
    if (step <= 0)
    {
        step = 1;
    }

    if (delta > 0)
    {
        if (delta < step)
        {
            step = delta;
        }
        context->menuVisualPosition = context->menuVisualPosition + step;
    }
    else
    {
        if (-delta < step)
        {
            step = -delta;
        }
        context->menuVisualPosition = context->menuVisualPosition - step;
    }

    context->refreshRequired = 1U;
}

static const char *Menu_GetMenuLabel(MenuOption option)
{
    switch (option)
    {
        case MENU_STOPWATCH:
            return "STOP";

        case MENU_FLASHLIGHT:
            return "LIGHT";

        case MENU_SETTINGS:
            return "SET";

        case MENU_BACK:
        default:
            return "BACK";
    }
}

static void Menu_DrawStopwatchIcon(int16_t centerX, int16_t bottomY, uint8_t scalePercent)
{
    uint8_t radius = Menu_ScaleValue(8U, scalePercent);
    uint8_t crownWidth = Menu_ScaleValue(4U, scalePercent);
    uint8_t crownHeight = Menu_ScaleValue(3U, scalePercent);
    uint8_t sideWidth = Menu_ScaleValue(3U, scalePercent);
    uint8_t sideHeight = Menu_ScaleValue(2U, scalePercent);
    int16_t centerY = bottomY - radius;

    OLED_DrawCircle(centerX, centerY, radius, 1U);
    OLED_DrawFilledRect(centerX - (int16_t)(crownWidth / 2U),
                        centerY - radius - crownHeight - 1,
                        crownWidth,
                        crownHeight,
                        1U);
    OLED_DrawFilledRect(centerX - (int16_t)(radius / 2U) - (int16_t)(sideWidth / 2U),
                        centerY - radius + 1,
                        sideWidth,
                        sideHeight,
                        1U);
    OLED_DrawFilledRect(centerX + (int16_t)(radius / 2U) - (int16_t)(sideWidth / 2U),
                        centerY - radius + 1,
                        sideWidth,
                        sideHeight,
                        1U);
    OLED_DrawLine(centerX, centerY, centerX, centerY - (int16_t)(radius / 2U), 1U);
    OLED_DrawLine(centerX,
                  centerY,
                  centerX + (int16_t)(radius / 3U) + 1,
                  centerY + (int16_t)(radius / 3U),
                  1U);
}

static void Menu_DrawLightIcon(int16_t centerX, int16_t bottomY, uint8_t scalePercent)
{
    uint8_t radius = Menu_ScaleValue(7U, scalePercent);
    uint8_t baseWidth = Menu_ScaleValue(8U, scalePercent);
    uint8_t baseHeight = Menu_ScaleValue(4U, scalePercent);
    uint8_t rayLength = Menu_ScaleValue(4U, scalePercent);
    int16_t centerY = bottomY - radius - 2;

    OLED_DrawCircle(centerX, centerY, radius, 1U);
    OLED_DrawLine(centerX - (int16_t)(radius / 2U), centerY + (int16_t)(radius / 2U),
                  centerX + (int16_t)(radius / 2U), centerY + (int16_t)(radius / 2U), 1U);
    OLED_DrawFilledRect(centerX - (int16_t)(baseWidth / 2U),
                        centerY + radius,
                        baseWidth,
                        baseHeight,
                        1U);
    OLED_DrawLine(centerX - 2, centerY + 1, centerX, centerY + 4, 1U);
    OLED_DrawLine(centerX + 2, centerY + 1, centerX, centerY + 4, 1U);
    OLED_DrawLine(centerX, centerY - radius - 2, centerX, centerY - radius - rayLength, 1U);
    OLED_DrawLine(centerX - radius - 2, centerY, centerX - radius - rayLength, centerY, 1U);
    OLED_DrawLine(centerX + radius + 2, centerY, centerX + radius + rayLength, centerY, 1U);
    OLED_DrawLine(centerX - radius + 1,
                  centerY - radius + 1,
                  centerX - radius - rayLength + 2,
                  centerY - radius - rayLength + 2,
                  1U);
    OLED_DrawLine(centerX + radius - 1,
                  centerY - radius + 1,
                  centerX + radius + rayLength - 2,
                  centerY - radius - rayLength + 2,
                  1U);
}

static void Menu_DrawBackIcon(int16_t centerX, int16_t bottomY, uint8_t scalePercent)
{
    uint8_t boxWidth = Menu_ScaleValue(20U, scalePercent);
    uint8_t boxHeight = Menu_ScaleValue(14U, scalePercent);
    uint8_t arrowWidth = Menu_ScaleValue(9U, scalePercent);
    uint8_t arrowHeight = Menu_ScaleValue(5U, scalePercent);
    int16_t topY = bottomY - boxHeight;
    int16_t midY = topY + (int16_t)(boxHeight / 2U);
    int16_t leftX = centerX - (int16_t)(boxWidth / 2U);
    int16_t rightX = centerX + (int16_t)(boxWidth / 2U) - 1;
    int16_t shaftStartX = centerX - (int16_t)(arrowWidth / 2U);
    int16_t shaftEndX = centerX + (int16_t)(arrowWidth / 2U);

    OLED_DrawRect(leftX, topY, boxWidth, boxHeight, 1U);
    OLED_DrawLine(shaftEndX, midY, shaftStartX, midY, 1U);
    OLED_DrawLine(shaftStartX, midY, shaftStartX + arrowHeight, midY - arrowHeight, 1U);
    OLED_DrawLine(shaftStartX, midY, shaftStartX + arrowHeight, midY + arrowHeight, 1U);
    OLED_DrawLine(rightX - 2, topY + 3, rightX - 2, topY + boxHeight - 4, 1U);
}

static void Menu_DrawSettingsIcon(int16_t centerX, int16_t bottomY, uint8_t scalePercent)
{
    uint8_t outerRadius = Menu_ScaleValue(9U, scalePercent);
    uint8_t innerRadius = Menu_ScaleValue(3U, scalePercent);
    uint8_t toothLength = Menu_ScaleValue(3U, scalePercent);
    uint8_t toothWidth = Menu_ScaleValue(2U, scalePercent);
    int16_t centerY = bottomY - outerRadius;

    OLED_DrawCircle(centerX, centerY, outerRadius, 1U);
    OLED_DrawCircle(centerX, centerY, innerRadius, 1U);

    OLED_DrawFilledRect(centerX - (int16_t)(toothWidth / 2U),
                        centerY - outerRadius - toothLength,
                        toothWidth,
                        toothLength,
                        1U);
    OLED_DrawFilledRect(centerX - (int16_t)(toothWidth / 2U),
                        centerY + outerRadius + 1,
                        toothWidth,
                        toothLength,
                        1U);
    OLED_DrawFilledRect(centerX - outerRadius - toothLength,
                        centerY - (int16_t)(toothWidth / 2U),
                        toothLength,
                        toothWidth,
                        1U);
    OLED_DrawFilledRect(centerX + outerRadius + 1,
                        centerY - (int16_t)(toothWidth / 2U),
                        toothLength,
                        toothWidth,
                        1U);
    OLED_DrawFilledRect(centerX - outerRadius + 1,
                        centerY - outerRadius + 1,
                        toothWidth,
                        toothWidth,
                        1U);
    OLED_DrawFilledRect(centerX + outerRadius - toothWidth,
                        centerY - outerRadius + 1,
                        toothWidth,
                        toothWidth,
                        1U);
    OLED_DrawFilledRect(centerX - outerRadius + 1,
                        centerY + outerRadius - toothWidth,
                        toothWidth,
                        toothWidth,
                        1U);
    OLED_DrawFilledRect(centerX + outerRadius - toothWidth,
                        centerY + outerRadius - toothWidth,
                        toothWidth,
                        toothWidth,
                        1U);
}

static void Menu_DrawMenuIcon(MenuOption option, int16_t centerX, int16_t bottomY, uint8_t scalePercent)
{
    switch (option)
    {
        case MENU_STOPWATCH:
            Menu_DrawStopwatchIcon(centerX, bottomY, scalePercent);
            break;

        case MENU_FLASHLIGHT:
            Menu_DrawLightIcon(centerX, bottomY, scalePercent);
            break;

        case MENU_SETTINGS:
            Menu_DrawSettingsIcon(centerX, bottomY, scalePercent);
            break;

        case MENU_BACK:
        default:
            Menu_DrawBackIcon(centerX, bottomY, scalePercent);
            break;
    }
}

static void Menu_DrawMenuIndicators(const MenuContext *context)
{
    uint8_t index;

    for (index = 0; index < MENU_OPTION_COUNT; index++)
    {
        int16_t relativePosition = Menu_GetMenuRelativePosition(index, context->menuVisualPosition);
        int16_t distance = Menu_AbsInt16(relativePosition);
        uint8_t width;
        uint8_t height;
        int16_t dotX;
        int16_t dotY = 1;

        if (distance < MENU_ANIMATION_UNIT)
        {
            uint16_t focusStrength = (uint16_t)(MENU_ANIMATION_UNIT - distance);
            width = (uint8_t)(3U + (focusStrength * 2U) / MENU_ANIMATION_UNIT);
            height = 2U;
        }
        else
        {
            width = 2U;
            height = 2U;
        }

        dotX = 52 + index * 12 - (int16_t)(width / 2U);

        OLED_DrawFilledRect(dotX, dotY, width, height, 1U);
    }
}

static const char *Menu_GetWeekdayText(uint8_t weekday)
{
    static const char *weekdayText[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

    if (weekday > 6U)
    {
        weekday = 0U;
    }

    return weekdayText[weekday];
}

static uint8_t Menu_GetMonthDays(uint16_t year, uint8_t month)
{
    static const uint8_t monthDays[12] =
    {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((month < 1U) || (month > 12U))
    {
        return 31U;
    }

    if ((month == 2U) && (RTC_IsLeapYear(year) != 0U))
    {
        return 29U;
    }

    return monthDays[month - 1U];
}

static void Menu_FormatDateString(const Time_t *time, char *dateString)
{
    const char *weekdayText = Menu_GetWeekdayText(time->weekday);

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
    dateString[11] = weekdayText[0];
    dateString[12] = weekdayText[1];
    dateString[13] = weekdayText[2];
    dateString[14] = '\0';
}

static void Menu_FormatTimeString(const Time_t *time, char *timeString)
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

static void Menu_FormatTwoDigitValue(uint8_t value, char *valueString)
{
    valueString[0] = (char)('0' + value / 10U);
    valueString[1] = (char)('0' + value % 10U);
    valueString[2] = '\0';
}

static void Menu_FormatYearString(uint16_t value, char *valueString)
{
    valueString[0] = (char)('0' + (value / 1000U) % 10U);
    valueString[1] = (char)('0' + (value / 100U) % 10U);
    valueString[2] = (char)('0' + (value / 10U) % 10U);
    valueString[3] = (char)('0' + value % 10U);
    valueString[4] = '\0';
}

static void Menu_FormatStopwatchString(uint32_t elapsedMs, char *timeString)
{
    uint16_t minutes = (uint16_t)(elapsedMs / 60000UL);
    uint8_t seconds = (uint8_t)((elapsedMs / 1000UL) % 60UL);
    uint8_t centiseconds = (uint8_t)((elapsedMs / 10UL) % 100UL);

    if (minutes > 99U)
    {
        minutes = 99U;
        seconds = 59U;
        centiseconds = 99U;
    }

    timeString[0] = (char)('0' + minutes / 10U);
    timeString[1] = (char)('0' + minutes % 10U);
    timeString[2] = ':';
    timeString[3] = (char)('0' + seconds / 10U);
    timeString[4] = (char)('0' + seconds % 10U);
    timeString[5] = '.';
    timeString[6] = (char)('0' + centiseconds / 10U);
    timeString[7] = (char)('0' + centiseconds % 10U);
    timeString[8] = '\0';
}

static void Menu_ShowHomePage(const MenuContext *context, const Time_t *time)
{
    char dateString[15];
    char timeString[9];

    Menu_FormatDateString(time, dateString);
    Menu_FormatTimeString(time, timeString);

    OLED_ShowString(1, 1, "                ");
    OLED_ShowString(1, 2, dateString);
    OLED_ShowBigString(2, 0, timeString);

    OLED_ShowString(4, 1, "                ");
    OLED_ShowStringMode(4, 1, " MENU ", (context->homeOption == HOME_MENU) ? 1U : 0U);
    OLED_ShowStringMode(4, 12, " SET ", (context->homeOption == HOME_SETTINGS) ? 1U : 0U);
}

static void Menu_ShowMenuPage(const MenuContext *context)
{
    uint8_t index;

    OLED_ClearBuffer();
    Menu_DrawMenuIndicators(context);

    for (index = 0; index < MENU_OPTION_COUNT; index++)
    {
        const char *label = Menu_GetMenuLabel((MenuOption)index);
        int16_t relativePosition = Menu_GetMenuRelativePosition(index, context->menuVisualPosition);
        int16_t distance = Menu_AbsInt16(relativePosition);
        int16_t centerX = MENU_CAROUSEL_CENTER_X + (relativePosition * MENU_CAROUSEL_SPACING) / MENU_ANIMATION_UNIT;
        uint8_t iconScale = Menu_GetFocusScale(MENU_ICON_SIDE_SCALE, MENU_ICON_CENTER_SCALE, distance);
        uint8_t textScale = Menu_GetFocusScale(MENU_TEXT_SIDE_SCALE, MENU_TEXT_CENTER_SCALE, distance);
        uint16_t labelWidth = OLED_GetStringWidth(label, textScale);
        int16_t labelX = centerX - (int16_t)(labelWidth / 2U);

        if ((centerX < -24) || (centerX > ((int16_t)OLED_WIDTH + 24)))
        {
            continue;
        }

        Menu_DrawMenuIcon((MenuOption)index, centerX, MENU_ICON_BOTTOM_Y, iconScale);
        OLED_DrawStringScaled(labelX, MENU_LABEL_TOP_Y, label, textScale, 1U);
    }
}

static void Menu_ShowStopwatchValue(const MenuContext *context)
{
    char stopwatchString[9];

    Menu_FormatStopwatchString(context->stopwatchElapsedMs, stopwatchString);
    OLED_ShowBigString(2, 0, stopwatchString);
}

static void Menu_ShowStopwatchPage(const MenuContext *context)
{
    OLED_ShowString(1, 1, "   STOPWATCH    ");
    Menu_ShowStopwatchValue(context);

    OLED_ShowString(4, 1, "                ");
    OLED_ShowStringMode(4, 1,
                        (context->stopwatchRunning != 0U) ? " PAUS" : " RUN ",
                        (context->stopwatchOption == STOPWATCH_ACTION_TOGGLE) ? 1U : 0U);
    OLED_ShowStringMode(4, 7, " RST ",
                        (context->stopwatchOption == STOPWATCH_ACTION_RESET) ? 1U : 0U);
    OLED_ShowStringMode(4, 12, " BK ",
                        (context->stopwatchOption == STOPWATCH_ACTION_BACK) ? 1U : 0U);
}

static void Menu_ShowFlashlightPage(const MenuContext *context)
{
    OLED_ShowString(1, 1, "  FLASHLIGHT    ");
    OLED_ShowString(2, 1, "                ");
    OLED_ShowString(2, 1, (LED_GetState() != 0U) ? "    LED: ON     " : "    LED: OFF    ");
    OLED_ShowString(3, 1, "                ");

    OLED_ShowString(4, 1, "                ");
    OLED_ShowStringMode(4, 1, " ON ",
                        (context->flashlightOption == FLASHLIGHT_ACTION_ON) ? 1U : 0U);
    OLED_ShowStringMode(4, 6, " OFF ",
                        (context->flashlightOption == FLASHLIGHT_ACTION_OFF) ? 1U : 0U);
    OLED_ShowStringMode(4, 12, "BACK",
                        (context->flashlightOption == FLASHLIGHT_ACTION_BACK) ? 1U : 0U);
}

static uint8_t Menu_IsTimeSettingField(TimeSettingOption option)
{
    return (option <= TIMESETTING_ITEM_SECOND) ? 1U : 0U;
}

static uint8_t Menu_IsDateSettingField(DateSettingOption option)
{
    return (option <= DATESETTING_ITEM_DAY) ? 1U : 0U;
}

static void Menu_RefreshSettingWeekday(MenuContext *context)
{
    context->timeSettingBuffer.weekday = RTC_CalculateWeekday(context->timeSettingBuffer.year,
                                                              context->timeSettingBuffer.month,
                                                              context->timeSettingBuffer.day);
}

static void Menu_ClampSettingDate(MenuContext *context)
{
    uint8_t maxDay = Menu_GetMonthDays(context->timeSettingBuffer.year, context->timeSettingBuffer.month);

    if (context->timeSettingBuffer.day > maxDay)
    {
        context->timeSettingBuffer.day = maxDay;
    }

    Menu_RefreshSettingWeekday(context);
}

static void Menu_ShowSettingSelectPage(const MenuContext *context)
{
    OLED_ShowString(1, 1, "   SETTINGS     ");
    OLED_ShowString(2, 1, "  PICK TARGET   ");

    OLED_ShowString(3, 1, "                ");
    OLED_ShowStringMode(3, 2, "TIME", (context->settingRootOption == SETTING_ROOT_TIME) ? 1U : 0U);
    OLED_ShowStringMode(3, 10, "DATE", (context->settingRootOption == SETTING_ROOT_DATE) ? 1U : 0U);

    OLED_ShowString(4, 1, "                ");
    OLED_ShowStringMode(4, 6, "BACK", (context->settingRootOption == SETTING_ROOT_BACK) ? 1U : 0U);
}

static void Menu_ShowTimeSettingPage(const MenuContext *context)
{
    char hourString[3];
    char minuteString[3];
    char secondString[3];
    uint8_t hourSelected = (context->timeSettingOption == TIMESETTING_ITEM_HOUR) ? 1U : 0U;
    uint8_t minuteSelected = (context->timeSettingOption == TIMESETTING_ITEM_MINUTE) ? 1U : 0U;
    uint8_t secondSelected = (context->timeSettingOption == TIMESETTING_ITEM_SECOND) ? 1U : 0U;

    Menu_FormatTwoDigitValue(context->timeSettingBuffer.hour, hourString);
    Menu_FormatTwoDigitValue(context->timeSettingBuffer.minute, minuteString);
    Menu_FormatTwoDigitValue(context->timeSettingBuffer.second, secondString);

    OLED_ShowString(1, 1, "                ");
    OLED_ShowString(1, 1, (context->timeSettingEditMode != 0U) ? "   EDIT TIME    " : "   SET TIME     ");

    OLED_ShowString(2, 1, "                ");
    OLED_ShowStringMode(2, 1, "HOUR", hourSelected);
    OLED_ShowStringMode(2, 7, "MIN", minuteSelected);
    OLED_ShowStringMode(2, 12, "SEC", secondSelected);

    OLED_ShowString(3, 1, "                ");
    OLED_ShowStringMode(3, 2, hourString, (uint8_t)(hourSelected && (context->timeSettingEditMode != 0U)));
    OLED_ShowStringMode(3, 7, minuteString, (uint8_t)(minuteSelected && (context->timeSettingEditMode != 0U)));
    OLED_ShowStringMode(3, 12, secondString, (uint8_t)(secondSelected && (context->timeSettingEditMode != 0U)));

    OLED_ShowString(4, 1, "                ");
    OLED_ShowStringMode(4, 2, "SAVE", (context->timeSettingOption == TIMESETTING_ITEM_SAVE) ? 1U : 0U);
    OLED_ShowStringMode(4, 10, "BACK", (context->timeSettingOption == TIMESETTING_ITEM_BACK) ? 1U : 0U);
}

static void Menu_ShowDateSettingPage(const MenuContext *context)
{
    char headerString[17] = "                ";
    char yearString[5];
    char monthString[3];
    char dayString[3];
    const char *weekdayText = Menu_GetWeekdayText(context->timeSettingBuffer.weekday);
    uint8_t yearSelected = (context->dateSettingOption == DATESETTING_ITEM_YEAR) ? 1U : 0U;
    uint8_t monthSelected = (context->dateSettingOption == DATESETTING_ITEM_MONTH) ? 1U : 0U;
    uint8_t daySelected = (context->dateSettingOption == DATESETTING_ITEM_DAY) ? 1U : 0U;

    Menu_FormatYearString(context->timeSettingBuffer.year, yearString);
    Menu_FormatTwoDigitValue(context->timeSettingBuffer.month, monthString);
    Menu_FormatTwoDigitValue(context->timeSettingBuffer.day, dayString);

    headerString[1] = 'D';
    headerString[2] = 'A';
    headerString[3] = 'T';
    headerString[4] = 'E';
    headerString[6] = weekdayText[0];
    headerString[7] = weekdayText[1];
    headerString[8] = weekdayText[2];
    if (context->timeSettingEditMode != 0U)
    {
        headerString[11] = 'E';
        headerString[12] = 'D';
        headerString[13] = 'I';
        headerString[14] = 'T';
    }
    else
    {
        headerString[11] = 'S';
        headerString[12] = 'E';
        headerString[13] = 'L';
    }
    OLED_ShowString(1, 1, headerString);

    OLED_ShowString(2, 1, "                ");
    OLED_ShowStringMode(2, 1, "YEAR", yearSelected);
    OLED_ShowStringMode(2, 8, "MON", monthSelected);
    OLED_ShowStringMode(2, 13, "DAY", daySelected);

    OLED_ShowString(3, 1, "                ");
    OLED_ShowStringMode(3, 1, yearString, (uint8_t)(yearSelected && (context->timeSettingEditMode != 0U)));
    OLED_ShowStringMode(3, 9, monthString, (uint8_t)(monthSelected && (context->timeSettingEditMode != 0U)));
    OLED_ShowStringMode(3, 14, dayString, (uint8_t)(daySelected && (context->timeSettingEditMode != 0U)));

    OLED_ShowString(4, 1, "                ");
    OLED_ShowStringMode(4, 2, "SAVE", (context->dateSettingOption == DATESETTING_ITEM_SAVE) ? 1U : 0U);
    OLED_ShowStringMode(4, 10, "BACK", (context->dateSettingOption == DATESETTING_ITEM_BACK) ? 1U : 0U);
}

static void Menu_RequestRender(MenuContext *context)
{
    context->refreshRequired = 1U;
    context->stopwatchTimeOnlyRefresh = 0U;
}

static void Menu_ChangeState(MenuContext *context, SystemState nextState)
{
    if (context->currentState != nextState)
    {
        context->currentState = nextState;
    }

    if (nextState == STATE_MENU)
    {
        Menu_SyncMenuAnimation(context);
    }

    context->refreshRequired = 1U;
    context->stopwatchTimeOnlyRefresh = 0U;
}

static void Menu_EnterSettingSelect(MenuContext *context, const Time_t *time)
{
    if (time != 0)
    {
        context->timeSettingBuffer = *time;
    }

    Menu_ClampSettingDate(context);
    context->settingPage = SETTING_PAGE_SELECT;
    context->settingRootOption = SETTING_ROOT_TIME;
    context->timeSettingOption = TIMESETTING_ITEM_HOUR;
    context->dateSettingOption = DATESETTING_ITEM_YEAR;
    context->timeSettingEditMode = 0U;
    Menu_ChangeState(context, STATE_TIMESETTING);
}

static void Menu_OpenTimeSetting(MenuContext *context, const Time_t *time)
{
    if (time != 0)
    {
        context->timeSettingBuffer = *time;
    }

    Menu_ClampSettingDate(context);
    context->settingPage = SETTING_PAGE_TIME;
    context->timeSettingOption = TIMESETTING_ITEM_HOUR;
    context->timeSettingEditMode = 0U;
    Menu_RequestRender(context);
}

static void Menu_OpenDateSetting(MenuContext *context, const Time_t *time)
{
    if (time != 0)
    {
        context->timeSettingBuffer = *time;
    }

    Menu_ClampSettingDate(context);
    context->settingPage = SETTING_PAGE_DATE;
    context->dateSettingOption = DATESETTING_ITEM_YEAR;
    context->timeSettingEditMode = 0U;
    Menu_RequestRender(context);
}

static void Menu_AdjustTimeSettingField(MenuContext *context, int16_t encoderStep)
{
    switch (context->timeSettingOption)
    {
        case TIMESETTING_ITEM_HOUR:
            context->timeSettingBuffer.hour = Menu_AdjustWrappedValue(context->timeSettingBuffer.hour, 0U, 23U, encoderStep);
            break;

        case TIMESETTING_ITEM_MINUTE:
            context->timeSettingBuffer.minute = Menu_AdjustWrappedValue(context->timeSettingBuffer.minute, 0U, 59U, encoderStep);
            break;

        case TIMESETTING_ITEM_SECOND:
            context->timeSettingBuffer.second = Menu_AdjustWrappedValue(context->timeSettingBuffer.second, 0U, 59U, encoderStep);
            break;

        case TIMESETTING_ITEM_SAVE:
        case TIMESETTING_ITEM_BACK:
        case TIMESETTING_ITEM_COUNT:
        default:
            return;
    }

    Menu_RequestRender(context);
}

static void Menu_AdjustDateSettingField(MenuContext *context, int16_t encoderStep)
{
    uint8_t maxDay;

    switch (context->dateSettingOption)
    {
        case DATESETTING_ITEM_YEAR:
            context->timeSettingBuffer.year = Menu_AdjustWrappedYear(context->timeSettingBuffer.year, 2000U, 2099U, encoderStep);
            Menu_ClampSettingDate(context);
            break;

        case DATESETTING_ITEM_MONTH:
            context->timeSettingBuffer.month = Menu_AdjustWrappedValue(context->timeSettingBuffer.month, 1U, 12U, encoderStep);
            Menu_ClampSettingDate(context);
            break;

        case DATESETTING_ITEM_DAY:
            maxDay = Menu_GetMonthDays(context->timeSettingBuffer.year, context->timeSettingBuffer.month);
            context->timeSettingBuffer.day = Menu_AdjustWrappedValue(context->timeSettingBuffer.day, 1U, maxDay, encoderStep);
            Menu_RefreshSettingWeekday(context);
            break;

        case DATESETTING_ITEM_SAVE:
        case DATESETTING_ITEM_BACK:
        case DATESETTING_ITEM_COUNT:
        default:
            return;
    }

    Menu_RequestRender(context);
}

static void Menu_SaveTimeSetting(MenuContext *context)
{
    Time_t updatedTime;

    RTC_GetTime(&updatedTime);
    updatedTime.hour = context->timeSettingBuffer.hour;
    updatedTime.minute = context->timeSettingBuffer.minute;
    updatedTime.second = context->timeSettingBuffer.second;
    RTC_SetTime(&updatedTime);
    context->lastHomeSecond = 0xFFU;
}

static void Menu_SaveDateSetting(MenuContext *context)
{
    Time_t updatedTime;

    RTC_GetTime(&updatedTime);
    updatedTime.year = context->timeSettingBuffer.year;
    updatedTime.month = context->timeSettingBuffer.month;
    updatedTime.day = context->timeSettingBuffer.day;
    updatedTime.weekday = RTC_CalculateWeekday(updatedTime.year, updatedTime.month, updatedTime.day);
    RTC_SetTime(&updatedTime);
    context->lastHomeSecond = 0xFFU;
}

static void Menu_UpdateStopwatch(MenuContext *context, uint32_t elapsedMs)
{
    uint32_t previousDisplayTick;
    uint8_t stoppedAtLimit = 0U;

    if ((context->stopwatchRunning == 0U) || (elapsedMs == 0U))
    {
        return;
    }

    previousDisplayTick = context->stopwatchElapsedMs / MENU_STOPWATCH_REFRESH_MS;

    if ((MENU_STOPWATCH_MAX_MS - context->stopwatchElapsedMs) <= elapsedMs)
    {
        context->stopwatchElapsedMs = MENU_STOPWATCH_MAX_MS;
        context->stopwatchRunning = 0U;
        stoppedAtLimit = 1U;
    }
    else
    {
        context->stopwatchElapsedMs += elapsedMs;
    }

    if (context->currentState != STATE_STOPWATCH)
    {
        return;
    }

    if (stoppedAtLimit != 0U)
    {
        Menu_RequestRender(context);
    }
    else if ((context->stopwatchElapsedMs / MENU_STOPWATCH_REFRESH_MS) != previousDisplayTick)
    {
        context->refreshRequired = 1U;
        context->stopwatchTimeOnlyRefresh = 1U;
    }
}

static void Menu_HandleEncoder(MenuContext *context, int16_t encoderStep)
{
    uint8_t updatedValue;

    if (encoderStep == 0)
    {
        return;
    }

    switch (context->currentState)
    {
        case STATE_HOME:
            updatedValue = Menu_MoveSelection((uint8_t)context->homeOption,
                                              HOME_OPTION_COUNT,
                                              encoderStep);
            if (updatedValue != (uint8_t)context->homeOption)
            {
                context->homeOption = (HomeOption)updatedValue;
                Menu_RequestRender(context);
            }
            break;

        case STATE_MENU:
            if (encoderStep != 0)
            {
                context->menuTargetPosition = context->menuTargetPosition + (int32_t)encoderStep * MENU_ANIMATION_UNIT;
                updatedValue = Menu_NormalizeIndex(context->menuTargetPosition / MENU_ANIMATION_UNIT,
                                                   MENU_OPTION_COUNT);
                context->menuOption = (MenuOption)updatedValue;
                Menu_RequestRender(context);
            }
            break;

        case STATE_STOPWATCH:
            updatedValue = Menu_MoveSelection((uint8_t)context->stopwatchOption,
                                              STOPWATCH_ACTION_COUNT,
                                              encoderStep);
            if (updatedValue != (uint8_t)context->stopwatchOption)
            {
                context->stopwatchOption = (StopwatchOption)updatedValue;
                Menu_RequestRender(context);
            }
            break;

        case STATE_FLASHLIGHT:
            updatedValue = Menu_MoveSelection((uint8_t)context->flashlightOption,
                                              FLASHLIGHT_ACTION_COUNT,
                                              encoderStep);
            if (updatedValue != (uint8_t)context->flashlightOption)
            {
                context->flashlightOption = (FlashlightOption)updatedValue;
                Menu_RequestRender(context);
            }
            break;

        case STATE_TIMESETTING:
            if (context->settingPage == SETTING_PAGE_SELECT)
            {
                updatedValue = Menu_MoveSelection((uint8_t)context->settingRootOption,
                                                  SETTING_ROOT_COUNT,
                                                  encoderStep);
                if (updatedValue != (uint8_t)context->settingRootOption)
                {
                    context->settingRootOption = (SettingRootOption)updatedValue;
                    Menu_RequestRender(context);
                }
            }
            else if (context->settingPage == SETTING_PAGE_TIME)
            {
                if (context->timeSettingEditMode != 0U)
                {
                    Menu_AdjustTimeSettingField(context, encoderStep);
                }
                else
                {
                    updatedValue = Menu_MoveSelection((uint8_t)context->timeSettingOption,
                                                      TIMESETTING_ITEM_COUNT,
                                                      encoderStep);
                    if (updatedValue != (uint8_t)context->timeSettingOption)
                    {
                        context->timeSettingOption = (TimeSettingOption)updatedValue;
                        Menu_RequestRender(context);
                    }
                }
            }
            else
            {
                if (context->timeSettingEditMode != 0U)
                {
                    Menu_AdjustDateSettingField(context, encoderStep);
                }
                else
                {
                    updatedValue = Menu_MoveSelection((uint8_t)context->dateSettingOption,
                                                      DATESETTING_ITEM_COUNT,
                                                      encoderStep);
                    if (updatedValue != (uint8_t)context->dateSettingOption)
                    {
                        context->dateSettingOption = (DateSettingOption)updatedValue;
                        Menu_RequestRender(context);
                    }
                }
            }
            break;

        default:
            break;
    }
}

static void Menu_HandleButton(MenuContext *context, const Time_t *time)
{
    switch (context->currentState)
    {
        case STATE_HOME:
            if (context->homeOption == HOME_MENU)
            {
                Menu_ChangeState(context, STATE_MENU);
            }
            else
            {
                Menu_EnterSettingSelect(context, time);
            }
            break;

        case STATE_MENU:
            if (context->menuOption == MENU_STOPWATCH)
            {
                context->stopwatchOption = STOPWATCH_ACTION_TOGGLE;
                Menu_ChangeState(context, STATE_STOPWATCH);
            }
            else if (context->menuOption == MENU_FLASHLIGHT)
            {
                context->flashlightOption = (LED_GetState() != 0U) ?
                                            FLASHLIGHT_ACTION_OFF :
                                            FLASHLIGHT_ACTION_ON;
                Menu_ChangeState(context, STATE_FLASHLIGHT);
            }
            else if (context->menuOption == MENU_SETTINGS)
            {
                Menu_EnterSettingSelect(context, time);
            }
            else
            {
                Menu_ChangeState(context, STATE_HOME);
            }
            break;

        case STATE_STOPWATCH:
            if (context->stopwatchOption == STOPWATCH_ACTION_TOGGLE)
            {
                context->stopwatchRunning = (context->stopwatchRunning == 0U) ? 1U : 0U;
                Menu_RequestRender(context);
            }
            else if (context->stopwatchOption == STOPWATCH_ACTION_RESET)
            {
                context->stopwatchRunning = 0U;
                context->stopwatchElapsedMs = 0U;
                Menu_RequestRender(context);
            }
            else
            {
                context->menuOption = MENU_STOPWATCH;
                Menu_ChangeState(context, STATE_MENU);
            }
            break;

        case STATE_FLASHLIGHT:
            if (context->flashlightOption == FLASHLIGHT_ACTION_ON)
            {
                LED_On();
                context->flashlightOption = FLASHLIGHT_ACTION_OFF;
                Menu_RequestRender(context);
            }
            else if (context->flashlightOption == FLASHLIGHT_ACTION_OFF)
            {
                LED_Off();
                context->flashlightOption = FLASHLIGHT_ACTION_ON;
                Menu_RequestRender(context);
            }
            else
            {
                context->menuOption = MENU_FLASHLIGHT;
                Menu_ChangeState(context, STATE_MENU);
            }
            break;

        case STATE_TIMESETTING:
            if (context->settingPage == SETTING_PAGE_SELECT)
            {
                if (context->settingRootOption == SETTING_ROOT_TIME)
                {
                    Menu_OpenTimeSetting(context, time);
                }
                else if (context->settingRootOption == SETTING_ROOT_DATE)
                {
                    Menu_OpenDateSetting(context, time);
                }
                else
                {
                    context->lastHomeSecond = 0xFFU;
                    Menu_ChangeState(context, STATE_HOME);
                }
            }
            else if (context->settingPage == SETTING_PAGE_TIME)
            {
                if (context->timeSettingEditMode != 0U)
                {
                    context->timeSettingEditMode = 0U;
                    Menu_RequestRender(context);
                }
                else if (Menu_IsTimeSettingField(context->timeSettingOption) != 0U)
                {
                    context->timeSettingEditMode = 1U;
                    Menu_RequestRender(context);
                }
                else if (context->timeSettingOption == TIMESETTING_ITEM_SAVE)
                {
                    Menu_SaveTimeSetting(context);
                    Menu_ChangeState(context, STATE_HOME);
                }
                else
                {
                    context->settingPage = SETTING_PAGE_SELECT;
                    context->settingRootOption = SETTING_ROOT_TIME;
                    Menu_RequestRender(context);
                }
            }
            else
            {
                if (context->timeSettingEditMode != 0U)
                {
                    context->timeSettingEditMode = 0U;
                    Menu_RequestRender(context);
                }
                else if (Menu_IsDateSettingField(context->dateSettingOption) != 0U)
                {
                    context->timeSettingEditMode = 1U;
                    Menu_RequestRender(context);
                }
                else if (context->dateSettingOption == DATESETTING_ITEM_SAVE)
                {
                    Menu_SaveDateSetting(context);
                    Menu_ChangeState(context, STATE_HOME);
                }
                else
                {
                    context->settingPage = SETTING_PAGE_SELECT;
                    context->settingRootOption = SETTING_ROOT_DATE;
                    Menu_RequestRender(context);
                }
            }
            break;

        default:
            break;
    }
}

void Menu_Init(MenuContext *context)
{
    if (context == 0)
    {
        return;
    }

    context->currentState = STATE_HOME;
    context->lastRenderedState = (SystemState)0xFF;
    context->homeOption = HOME_MENU;
    context->menuOption = MENU_STOPWATCH;
    context->stopwatchOption = STOPWATCH_ACTION_TOGGLE;
    context->flashlightOption = FLASHLIGHT_ACTION_ON;
    context->settingPage = SETTING_PAGE_SELECT;
    context->settingRootOption = SETTING_ROOT_TIME;
    context->timeSettingOption = TIMESETTING_ITEM_HOUR;
    context->dateSettingOption = DATESETTING_ITEM_YEAR;
    context->timeSettingBuffer.year = 2000U;
    context->timeSettingBuffer.month = 1U;
    context->timeSettingBuffer.day = 1U;
    context->timeSettingBuffer.hour = 0U;
    context->timeSettingBuffer.minute = 0U;
    context->timeSettingBuffer.second = 0U;
    context->timeSettingBuffer.weekday = 6U;
    context->refreshRequired = 1U;
    context->stopwatchTimeOnlyRefresh = 0U;
    context->lastHomeSecond = 0xFFU;
    context->stopwatchRunning = 0U;
    context->timeSettingEditMode = 0U;
    context->menuAnimationAccumulatorMs = 0U;
    context->stopwatchElapsedMs = 0U;
    Menu_SyncMenuAnimation(context);
}

void Menu_Update(MenuContext *context,
                 const Time_t *time,
                 uint32_t elapsedMs,
                 int16_t encoderStep,
                 uint8_t buttonEvent)
{
    if ((context == 0) || (time == 0))
    {
        return;
    }

    Menu_UpdateStopwatch(context, elapsedMs);
    Menu_HandleEncoder(context, encoderStep);
    Menu_UpdateMenuAnimation(context, elapsedMs);

    if (buttonEvent != 0U)
    {
        Menu_HandleButton(context, time);
    }

    if ((context->currentState == STATE_HOME) && (time->second != context->lastHomeSecond))
    {
        context->lastHomeSecond = time->second;
        Menu_RequestRender(context);
    }
}

uint8_t Menu_NeedsRender(const MenuContext *context)
{
    if (context == 0)
    {
        return 0U;
    }

    return context->refreshRequired;
}

void Menu_Render(MenuContext *context, const Time_t *time)
{
    if ((context == 0) || (time == 0) || (context->refreshRequired == 0U))
    {
        return;
    }

    if (context->lastRenderedState != context->currentState)
    {
        if (context->currentState != STATE_MENU)
        {
            OLED_Clear();
        }
        context->lastRenderedState = context->currentState;
        context->stopwatchTimeOnlyRefresh = 0U;
    }

    if ((context->currentState == STATE_STOPWATCH) && (context->stopwatchTimeOnlyRefresh != 0U))
    {
        Menu_ShowStopwatchValue(context);
    }
    else
    {
        switch (context->currentState)
        {
            case STATE_HOME:
                Menu_ShowHomePage(context, time);
                break;

            case STATE_MENU:
                Menu_ShowMenuPage(context);
                OLED_Update();
                break;

            case STATE_STOPWATCH:
                Menu_ShowStopwatchPage(context);
                break;

            case STATE_FLASHLIGHT:
                Menu_ShowFlashlightPage(context);
                break;

            case STATE_TIMESETTING:
            default:
                if (context->settingPage == SETTING_PAGE_SELECT)
                {
                    Menu_ShowSettingSelectPage(context);
                }
                else if (context->settingPage == SETTING_PAGE_TIME)
                {
                    Menu_ShowTimeSettingPage(context);
                }
                else
                {
                    Menu_ShowDateSettingPage(context);
                }
                break;
        }
    }

    context->refreshRequired = 0U;
    context->stopwatchTimeOnlyRefresh = 0U;
}
