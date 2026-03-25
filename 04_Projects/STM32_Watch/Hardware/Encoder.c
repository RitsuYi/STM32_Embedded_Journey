#include "Encoder.h"

static int16_t Encoder_TransitionCount;
static uint8_t Encoder_PreviousState;

static uint8_t Encoder_ReadState(void)
{
    uint8_t channelA = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == Bit_SET) ? 1U : 0U;
    uint8_t channelB = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_SET) ? 1U : 0U;

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
    uint8_t tableIndex;

    if (currentState == Encoder_PreviousState)
    {
        return;
    }

    tableIndex = (uint8_t)((Encoder_PreviousState << 2) | currentState);
    Encoder_TransitionCount += transitionTable[tableIndex];
    Encoder_PreviousState = currentState;
}

void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    Encoder_Reset();
}

int16_t Encoder_Get(void)
{
    int16_t stepCount;

    Encoder_UpdateState();

    stepCount = Encoder_TransitionCount / 4;
    Encoder_TransitionCount -= (int16_t)(stepCount * 4);

    return stepCount;
}

void Encoder_Reset(void)
{
    Encoder_TransitionCount = 0;
    Encoder_PreviousState = Encoder_ReadState();
}

void Encoder_HandleIRQ(void)
{
    Encoder_UpdateState();
}
