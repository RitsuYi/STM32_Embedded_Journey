#include "GraySensor.h"
#include "BoardConfig.h"

#if ((BOARD_GRAY_SENSOR_ACTIVE_LEVEL != 0U) && (BOARD_GRAY_SENSOR_ACTIVE_LEVEL != 1U))
#error "BOARD_GRAY_SENSOR_ACTIVE_LEVEL must be 0 or 1"
#endif

static const uint16_t g_graySensorPins[GRAY_SENSOR_COUNT] =
{
	BOARD_GRAY_SENSOR_LEFT2_PIN,
	BOARD_GRAY_SENSOR_LEFT1_PIN,
	BOARD_GRAY_SENSOR_CENTER_PIN,
	BOARD_GRAY_SENSOR_RIGHT1_PIN,
	BOARD_GRAY_SENSOR_RIGHT2_PIN
};

static uint8_t GraySensor_IsValidPosition(GraySensor_Position_t position)
{
	return (uint8_t)(position < GRAY_SENSOR_COUNT);
}

static uint8_t GraySensor_ReadPin(uint16_t pin)
{
	if (GPIO_ReadInputDataBit(BOARD_GRAY_SENSOR_GPIO_PORT, pin) == Bit_SET)
	{
		return 1U;
	}

	return 0U;
}

static uint8_t GraySensor_NormalizeLevel(uint8_t rawLevel)
{
#if (BOARD_GRAY_SENSOR_ACTIVE_LEVEL == 0U)
	return (uint8_t)(rawLevel == 0U);
#else
	return rawLevel;
#endif
}

void GraySensor_Init(void)
{
	GPIO_InitTypeDef gpioInitStructure;

	RCC_AHB1PeriphClockCmd(BOARD_GRAY_SENSOR_GPIO_RCC, ENABLE);

	gpioInitStructure.GPIO_Pin = BOARD_GRAY_SENSOR_ALL_PINS;
	gpioInitStructure.GPIO_Mode = GPIO_Mode_IN;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_PuPd = BOARD_GRAY_SENSOR_GPIO_PUPD;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_GRAY_SENSOR_GPIO_PORT, &gpioInitStructure);
}

uint8_t GraySensor_ReadRaw(GraySensor_Position_t position)
{
	if (GraySensor_IsValidPosition(position) == 0U)
	{
		return 0U;
	}

	return GraySensor_ReadPin(g_graySensorPins[(uint8_t)position]);
}

uint8_t GraySensor_Read(GraySensor_Position_t position)
{
	return GraySensor_NormalizeLevel(GraySensor_ReadRaw(position));
}

uint8_t GraySensor_GetRawMask(void)
{
	uint8_t mask;

	mask = 0U;
	if (GraySensor_ReadRaw(GRAY_SENSOR_LEFT_2) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_LEFT_2;
	}
	if (GraySensor_ReadRaw(GRAY_SENSOR_LEFT_1) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_LEFT_1;
	}
	if (GraySensor_ReadRaw(GRAY_SENSOR_CENTER) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_CENTER;
	}
	if (GraySensor_ReadRaw(GRAY_SENSOR_RIGHT_1) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_RIGHT_1;
	}
	if (GraySensor_ReadRaw(GRAY_SENSOR_RIGHT_2) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_RIGHT_2;
	}

	return mask;
}

uint8_t GraySensor_GetMask(void)
{
	uint8_t mask;

	mask = 0U;
	if (GraySensor_Read(GRAY_SENSOR_LEFT_2) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_LEFT_2;
	}
	if (GraySensor_Read(GRAY_SENSOR_LEFT_1) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_LEFT_1;
	}
	if (GraySensor_Read(GRAY_SENSOR_CENTER) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_CENTER;
	}
	if (GraySensor_Read(GRAY_SENSOR_RIGHT_1) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_RIGHT_1;
	}
	if (GraySensor_Read(GRAY_SENSOR_RIGHT_2) != 0U)
	{
		mask |= GRAY_SENSOR_MASK_RIGHT_2;
	}

	return mask;
}

uint8_t GraySensor_HasLine(void)
{
	return (uint8_t)(GraySensor_GetMask() != 0U);
}

int16_t GraySensor_GetLinePosition(void)
{
	uint8_t mask;
	int32_t weightedSum;
	int32_t activeCount;

	mask = GraySensor_GetMask();
	weightedSum = 0;
	activeCount = 0;

	if ((mask & GRAY_SENSOR_MASK_LEFT_2) != 0U)
	{
		weightedSum -= 200;
		activeCount++;
	}
	if ((mask & GRAY_SENSOR_MASK_LEFT_1) != 0U)
	{
		weightedSum -= 100;
		activeCount++;
	}
	if ((mask & GRAY_SENSOR_MASK_CENTER) != 0U)
	{
		activeCount++;
	}
	if ((mask & GRAY_SENSOR_MASK_RIGHT_1) != 0U)
	{
		weightedSum += 100;
		activeCount++;
	}
	if ((mask & GRAY_SENSOR_MASK_RIGHT_2) != 0U)
	{
		weightedSum += 200;
		activeCount++;
	}

	if (activeCount == 0)
	{
		return GRAY_SENSOR_LINE_POSITION_NONE;
	}

	if (weightedSum >= 0)
	{
		return (int16_t)((weightedSum + (activeCount / 2)) / activeCount);
	}

	return (int16_t)((weightedSum - (activeCount / 2)) / activeCount);
}
