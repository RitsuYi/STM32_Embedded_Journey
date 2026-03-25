#include "stm32f10x.h"
#include "Button.h"
#include "Encoder.h"
#include "LED.h"
#include "Menu.h"
#include "OLED.h"
#include "RTC.h"
#include "Timer.h"

int main(void)
{
    Time_t currentTime;
    uint32_t lastTick = 0U;
    MenuContext menuContext;

    OLED_Init();
    Timer_Init();
    Button_Init();
    Encoder_Init();
    LED_Init();
    RTC_Init();

    Menu_Init(&menuContext);
    lastTick = Timer_GetMillis();
    RTC_GetTime(&currentTime);
    Menu_Render(&menuContext, &currentTime);

    while (1)
    {
        uint32_t currentMillis = Timer_GetMillis();
        uint32_t elapsedMs = currentMillis - lastTick;
        int16_t encoderStep = Encoder_Get();
        uint8_t buttonEvent = Button_GetEvent();

        if (elapsedMs != 0U)
        {
            lastTick = currentMillis;
            RTC_AdvanceMilliseconds(elapsedMs);
        }

        RTC_GetTime(&currentTime);
        Menu_Update(&menuContext, &currentTime, elapsedMs, encoderStep, buttonEvent);
        RTC_GetTime(&currentTime);

        if (Menu_NeedsRender(&menuContext) != 0U)
        {
            Menu_Render(&menuContext, &currentTime);
        }
    }
}
