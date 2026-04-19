#include "Delay.h"

static void Delay_SysTick(uint32_t ticks)
{
	if (ticks == 0U)
	{
		return;
	}

	SysTick->LOAD = ticks - 1U;
	SysTick->VAL = 0U;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
	while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U);
	SysTick->CTRL = 0U;
}

void Delay_us(uint32_t xus)
{
	uint32_t ticks_per_us = SystemCoreClock / 1000000U;
	uint32_t max_us;

	if (ticks_per_us == 0U)
	{
		return;
	}

	max_us = 0xFFFFFFU / ticks_per_us;
	while (xus > max_us)
	{
		Delay_SysTick(max_us * ticks_per_us);
		xus -= max_us;
	}

	Delay_SysTick(xus * ticks_per_us);
}

void Delay_ms(uint32_t xms)
{
	while (xms--)
	{
		Delay_us(1000U);
	}
}

void Delay_s(uint32_t xs)
{
	while (xs--)
	{
		Delay_ms(1000U);
	}
}
