#include "Timer.h"

static uint32_t Timer_BaseTicks;

static void Timer_UpdateOverflow(void)
{
    if (TIM_GetFlagStatus(TIM2, TIM_FLAG_Update) != RESET)
    {
        TIM_ClearFlag(TIM2, TIM_FLAG_Update);
        Timer_BaseTicks += 65536UL;
    }
}

void Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 65535U;
    TIM_TimeBaseStructure.TIM_Prescaler = 720U - 1U;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_SetCounter(TIM2, 0U);
    Timer_BaseTicks = 0U;
    TIM_Cmd(TIM2, ENABLE);
}

uint32_t Timer_GetMillis(void)
{
    uint16_t counterValue;
    uint32_t totalTicks;

    Timer_UpdateOverflow();
    counterValue = TIM_GetCounter(TIM2);
    Timer_UpdateOverflow();
    totalTicks = Timer_BaseTicks + counterValue;

    return totalTicks / 100U;
}

void Timer_ResetMillis(void)
{
    TIM_Cmd(TIM2, DISABLE);
    TIM_SetCounter(TIM2, 0U);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    Timer_BaseTicks = 0U;
    TIM_Cmd(TIM2, ENABLE);
}

void Timer_IRQHandler(void)
{
}
