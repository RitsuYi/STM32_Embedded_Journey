#ifndef __WATCH_MENU_H
#define __WATCH_MENU_H

#include "RTC.h"

typedef enum
{
    STATE_HOME = 0,
    STATE_MENU,
    STATE_STOPWATCH,
    STATE_FLASHLIGHT,
    STATE_TIMESETTING
} SystemState;

typedef enum
{
    HOME_MENU = 0,
    HOME_SETTINGS,
    HOME_OPTION_COUNT
} HomeOption;

typedef enum
{
    MENU_STOPWATCH = 0,
    MENU_FLASHLIGHT,
    MENU_SETTINGS,
    MENU_BACK,
    MENU_OPTION_COUNT
} MenuOption;

typedef enum
{
    STOPWATCH_ACTION_TOGGLE = 0,
    STOPWATCH_ACTION_RESET,
    STOPWATCH_ACTION_BACK,
    STOPWATCH_ACTION_COUNT
} StopwatchOption;

typedef enum
{
    FLASHLIGHT_ACTION_ON = 0,
    FLASHLIGHT_ACTION_OFF,
    FLASHLIGHT_ACTION_BACK,
    FLASHLIGHT_ACTION_COUNT
} FlashlightOption;

typedef enum
{
    TIMESETTING_ITEM_HOUR = 0,
    TIMESETTING_ITEM_MINUTE,
    TIMESETTING_ITEM_SECOND,
    TIMESETTING_ITEM_SAVE,
    TIMESETTING_ITEM_BACK,
    TIMESETTING_ITEM_COUNT
} TimeSettingOption;

typedef enum
{
    DATESETTING_ITEM_YEAR = 0,
    DATESETTING_ITEM_MONTH,
    DATESETTING_ITEM_DAY,
    DATESETTING_ITEM_SAVE,
    DATESETTING_ITEM_BACK,
    DATESETTING_ITEM_COUNT
} DateSettingOption;

typedef enum
{
    SETTING_PAGE_SELECT = 0,
    SETTING_PAGE_TIME,
    SETTING_PAGE_DATE
} SettingPage;

typedef enum
{
    SETTING_ROOT_TIME = 0,
    SETTING_ROOT_DATE,
    SETTING_ROOT_BACK,
    SETTING_ROOT_COUNT
} SettingRootOption;

typedef struct
{
    SystemState currentState;
    SystemState lastRenderedState;
    HomeOption homeOption;
    MenuOption menuOption;
    StopwatchOption stopwatchOption;
    FlashlightOption flashlightOption;
    SettingPage settingPage;
    SettingRootOption settingRootOption;
    TimeSettingOption timeSettingOption;
    DateSettingOption dateSettingOption;
    Time_t timeSettingBuffer;
    uint8_t refreshRequired;
    uint8_t stopwatchTimeOnlyRefresh;
    uint8_t lastHomeSecond;
    uint8_t stopwatchRunning;
    uint8_t timeSettingEditMode;
    uint16_t menuAnimationAccumulatorMs;
    int32_t menuVisualPosition;
    int32_t menuTargetPosition;
    uint32_t stopwatchElapsedMs;
} MenuContext;

void Menu_Init(MenuContext *context);
void Menu_Update(MenuContext *context,
                 const Time_t *time,
                 uint32_t elapsedMs,
                 int16_t encoderStep,
                 uint8_t buttonEvent);
uint8_t Menu_NeedsRender(const MenuContext *context);
void Menu_Render(MenuContext *context, const Time_t *time);

#endif
