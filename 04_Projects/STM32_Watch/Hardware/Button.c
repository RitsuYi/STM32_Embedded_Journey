#include "Button.h"
#include "Delay.h"

void Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

uint8_t Button_GetEvent(void)
{
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_RESET)
    {
        Delay_ms(20);
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_RESET)
        {
            while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_RESET)
            {
            }

            Delay_ms(20);
            return 1;
        }
    }

    return 0;
}

uint8_t Button_IsPressed(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_RESET) ? 1U : 0U;
}
