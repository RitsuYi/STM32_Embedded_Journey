#include "Button.h"
#include "Timer.h"

static uint8_t Button_LastRawState = 1U;
static uint8_t Button_StableState = 1U;
static uint32_t Button_LastChangeTime;

static uint8_t Button_ReadRawState(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_SET) ? 1U : 0U;
}

void Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t initialState;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    initialState = Button_ReadRawState();
    Button_LastRawState = initialState;
    Button_StableState = initialState;
    Button_LastChangeTime = Timer_GetMillis();
}

uint8_t Button_GetEvent(void)
{
    uint8_t rawState = Button_ReadRawState();
    uint32_t currentTime = Timer_GetMillis();

    if (rawState != Button_LastRawState)
    {
        Button_LastRawState = rawState;
        Button_LastChangeTime = currentTime;
    }

    if ((uint32_t)(currentTime - Button_LastChangeTime) >= 20U)
    {
        if (Button_StableState != rawState)
        {
            Button_StableState = rawState;
            if (Button_StableState == 0U)
            {
                return 1U;
            }
        }
    }

    return 0U;
}
