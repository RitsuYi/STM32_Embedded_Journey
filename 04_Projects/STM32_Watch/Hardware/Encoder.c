#include "Encoder.h"

static volatile int16_t Encoder_TransitionCount;
static uint8_t Encoder_PreviousState;
static int16_t Encoder_Remainder;

static uint8_t Encoder_ReadState(void)
{
    uint8_t channelA = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) ? 1U : 0U;
    uint8_t channelB = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) ? 1U : 0U;

    return (uint8_t)((channelA << 1) | channelB);
}

static void Encoder_UpdateState(void)
{
    static const int8_t transitionTable[16] =
    {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };

    uint8_t currentState = Encoder_ReadState();
    uint8_t tableIndex = (uint8_t)((Encoder_PreviousState << 2) | currentState);

    Encoder_TransitionCount += transitionTable[tableIndex];
    Encoder_PreviousState = currentState;
}

void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource10);

    EXTI_ClearITPendingBit(EXTI_Line0 | EXTI_Line10);

    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_Init(&EXTI_InitStructure);

    EXTI_InitStructure.EXTI_Line = EXTI_Line10;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    Encoder_Reset();
}

int16_t Encoder_Get(void)
{
    int16_t transitionCount;
    int16_t stepCount;

    __disable_irq();
    transitionCount = Encoder_TransitionCount;
    Encoder_TransitionCount = 0;
    __enable_irq();

    transitionCount += Encoder_Remainder;
    stepCount = transitionCount / 4;
    Encoder_Remainder = transitionCount % 4;

    return stepCount;
}

void Encoder_Reset(void)
{
    __disable_irq();
    Encoder_TransitionCount = 0;
    Encoder_PreviousState = Encoder_ReadState();
    __enable_irq();

    Encoder_Remainder = 0;
}

void Encoder_HandleIRQ(void)
{
    Encoder_UpdateState();
}
