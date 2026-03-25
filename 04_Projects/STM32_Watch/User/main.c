#include "stm32f10x.h"
#include "Button.h"
#include "Encoder.h"
#include "LED.h"
#include "OLED.h"
#include "RTC.h"
#include "Timer.h"

static void Watch_ShowDebugPage(const Time_t *time, int16_t encoderTotal, uint16_t buttonCount, uint8_t ledState)
{
    OLED_ShowString(1, 1, "DATE");
    OLED_ShowNum(1, 6, time->year, 4);
    OLED_ShowChar(1, 10, '-');
    OLED_ShowNum(1, 11, time->month, 2);
    OLED_ShowChar(1, 13, '-');
    OLED_ShowNum(1, 14, time->day, 2);

    OLED_ShowString(2, 1, "TIME");
    OLED_ShowNum(2, 6, time->hour, 2);
    OLED_ShowChar(2, 8, ':');
    OLED_ShowNum(2, 9, time->minute, 2);
    OLED_ShowChar(2, 11, ':');
    OLED_ShowNum(2, 12, time->second, 2);

    OLED_ShowString(3, 1, "WK");
    OLED_ShowNum(3, 3, time->weekday, 1);
    OLED_ShowString(3, 5, "ENC");
    OLED_ShowSignedNum(3, 9, encoderTotal, 3);

    OLED_ShowString(4, 1, "BTN");
    OLED_ShowNum(4, 4, buttonCount % 100U, 2);
    OLED_ShowString(4, 7, "LED");
    OLED_ShowNum(4, 10, ledState, 1);
}

int main(void)
{
    Time_t currentTime;
    uint32_t lastRefreshTick = 0;
    int16_t encoderTotal = 0;
    uint16_t buttonCount = 0;
    uint8_t ledState = 0;

    OLED_Init();
    LED_Init();
    Button_Init();
    Encoder_Init();
    Timer_Init();
    RTC_Init();
    OLED_Clear();

    RTC_GetTime(&currentTime);
    Watch_ShowDebugPage(&currentTime, encoderTotal, buttonCount, ledState);

    while (1)
    {
        int16_t encoderStep = Encoder_Get();
        uint8_t buttonEvent = Button_GetEvent();

        if (encoderStep != 0)
        {
            encoderTotal += encoderStep;
            RTC_AdjustSeconds(encoderStep);
        }

        if (buttonEvent != 0U)
        {
            buttonCount++;
            LED_Toggle();
        }

        if ((Timer_GetMillis() - lastRefreshTick >= 200U) || (encoderStep != 0) || (buttonEvent != 0U))
        {
            lastRefreshTick = Timer_GetMillis();
            ledState = LED_GetState();
            RTC_GetTime(&currentTime);
            Watch_ShowDebugPage(&currentTime, encoderTotal, buttonCount, ledState);
        }
    }
}
