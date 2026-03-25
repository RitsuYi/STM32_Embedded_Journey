#include "LED.h"

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    LED_Off();
}

void LED_On(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_1);
}

void LED_Off(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_1);
}

void LED_Toggle(void)
{
    if (LED_GetState())
    {
        LED_Off();
    }
    else
    {
        LED_On();
    }
}

uint8_t LED_GetState(void)
{
    return (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == Bit_RESET) ? 1U : 0U;
}

void LED1_ON(void)
{
    LED_On();
}

void LED1_OFF(void)
{
    LED_Off();
}

void LED1_Turn(void)
{
    LED_Toggle();
}

void LED2_ON(void)
{
    LED_On();
}

void LED2_OFF(void)
{
    LED_Off();
}

void LED2_Turn(void)
{
    LED_Toggle();
}
