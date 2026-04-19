#include "Encoder.h"
#include "BoardConfig.h"

typedef struct
{
	TIM_TypeDef *timer;
	uint32_t timerRcc;
	uint8_t isApb2;
	uint8_t is32Bit;
	uint8_t reverseFlag;
	uint32_t gpioRccA;
	uint32_t gpioRccB;
	GPIO_TypeDef *portA;
	uint16_t pinA;
	uint8_t pinSourceA;
	GPIO_TypeDef *portB;
	uint16_t pinB;
	uint8_t pinSourceB;
	uint8_t gpioAf;
	uint32_t period;
	uint32_t counterCenter;
} Encoder_Config_t;

static const Encoder_Config_t g_encoderConfig[ENCODER_COUNT] =
{
	{
		BOARD_ENCODER_A_TIMER,
		BOARD_ENCODER_A_TIMER_RCC,
		BOARD_ENCODER_A_IS_APB2,
		BOARD_ENCODER_A_IS_32BIT,
		BOARD_ENCODER_A_REVERSE,
		BOARD_ENCODER_A_GPIO_RCC_A,
		BOARD_ENCODER_A_GPIO_RCC_B,
		BOARD_ENCODER_A_PORT_A,
		BOARD_ENCODER_A_PIN_A,
		BOARD_ENCODER_A_PINSOURCE_A,
		BOARD_ENCODER_A_PORT_B,
		BOARD_ENCODER_A_PIN_B,
		BOARD_ENCODER_A_PINSOURCE_B,
		BOARD_ENCODER_A_GPIO_AF,
		BOARD_ENCODER_16BIT_PERIOD,
		BOARD_ENCODER_16BIT_CENTER
	},
	{
		BOARD_ENCODER_B_TIMER,
		BOARD_ENCODER_B_TIMER_RCC,
		BOARD_ENCODER_B_IS_APB2,
		BOARD_ENCODER_B_IS_32BIT,
		BOARD_ENCODER_B_REVERSE,
		BOARD_ENCODER_B_GPIO_RCC_A,
		BOARD_ENCODER_B_GPIO_RCC_B,
		BOARD_ENCODER_B_PORT_A,
		BOARD_ENCODER_B_PIN_A,
		BOARD_ENCODER_B_PINSOURCE_A,
		BOARD_ENCODER_B_PORT_B,
		BOARD_ENCODER_B_PIN_B,
		BOARD_ENCODER_B_PINSOURCE_B,
		BOARD_ENCODER_B_GPIO_AF,
		BOARD_ENCODER_32BIT_PERIOD,
		BOARD_ENCODER_32BIT_CENTER
	},
	{
		BOARD_ENCODER_C_TIMER,
		BOARD_ENCODER_C_TIMER_RCC,
		BOARD_ENCODER_C_IS_APB2,
		BOARD_ENCODER_C_IS_32BIT,
		BOARD_ENCODER_C_REVERSE,
		BOARD_ENCODER_C_GPIO_RCC_A,
		BOARD_ENCODER_C_GPIO_RCC_B,
		BOARD_ENCODER_C_PORT_A,
		BOARD_ENCODER_C_PIN_A,
		BOARD_ENCODER_C_PINSOURCE_A,
		BOARD_ENCODER_C_PORT_B,
		BOARD_ENCODER_C_PIN_B,
		BOARD_ENCODER_C_PINSOURCE_B,
		BOARD_ENCODER_C_GPIO_AF,
		BOARD_ENCODER_16BIT_PERIOD,
		BOARD_ENCODER_16BIT_CENTER
	},
	{
		BOARD_ENCODER_D_TIMER,
		BOARD_ENCODER_D_TIMER_RCC,
		BOARD_ENCODER_D_IS_APB2,
		BOARD_ENCODER_D_IS_32BIT,
		BOARD_ENCODER_D_REVERSE,
		BOARD_ENCODER_D_GPIO_RCC_A,
		BOARD_ENCODER_D_GPIO_RCC_B,
		BOARD_ENCODER_D_PORT_A,
		BOARD_ENCODER_D_PIN_A,
		BOARD_ENCODER_D_PINSOURCE_A,
		BOARD_ENCODER_D_PORT_B,
		BOARD_ENCODER_D_PIN_B,
		BOARD_ENCODER_D_PINSOURCE_B,
		BOARD_ENCODER_D_GPIO_AF,
		BOARD_ENCODER_16BIT_PERIOD,
		BOARD_ENCODER_16BIT_CENTER
	}
};

static int32_t g_encoderCount[ENCODER_COUNT];
static int32_t g_encoderDeltaCount[ENCODER_COUNT];
static int32_t g_encoderSpeedRpm[ENCODER_COUNT];
static int32_t g_encoderDeltaHistory[ENCODER_COUNT][BOARD_ENCODER_SPEED_AVG_SAMPLES];
static int32_t g_encoderDeltaWindowSum[ENCODER_COUNT];
static uint8_t g_encoderHistoryIndex = 0U;
static uint8_t g_encoderValidSamples[ENCODER_COUNT];

typedef enum
{
	CAR_WHEEL_LEFT_FRONT = 0,
	CAR_WHEEL_RIGHT_FRONT,
	CAR_WHEEL_LEFT_REAR,
	CAR_WHEEL_RIGHT_REAR,
	CAR_WHEEL_COUNT
} Car_WheelPosition_t;

/* Keep wheel order fully driven by BoardConfig instead of hard-coded A/B/C/D. */
static const Encoder_Id_t g_carWheelEncoderMap[CAR_WHEEL_COUNT] =
{
	(Encoder_Id_t)BOARD_CAR_LEFT_FRONT_ENCODER,
	(Encoder_Id_t)BOARD_CAR_RIGHT_FRONT_ENCODER,
	(Encoder_Id_t)BOARD_CAR_LEFT_REAR_ENCODER,
	(Encoder_Id_t)BOARD_CAR_RIGHT_REAR_ENCODER
};

static int32_t Encoder_DivRoundClosest(int64_t numerator, int64_t denominator)
{
	if (denominator <= 0)
	{
		return 0;
	}

	if (numerator >= 0)
	{
		return (int32_t)((numerator + (denominator / 2)) / denominator);
	}

	return (int32_t)(-(((-numerator) + (denominator / 2)) / denominator));
}

static void Encoder_InitOne(Encoder_Id_t encoder)
{
	GPIO_InitTypeDef gpioInitStructure;
	TIM_TimeBaseInitTypeDef timeBaseInitStructure;
	TIM_ICInitTypeDef icInitStructure;
	const Encoder_Config_t *config;

	config = &g_encoderConfig[encoder];

	RCC_AHB1PeriphClockCmd(config->gpioRccA | config->gpioRccB, ENABLE);
	if (config->isApb2 != 0U)
	{
		RCC_APB2PeriphClockCmd(config->timerRcc, ENABLE);
	}
	else
	{
		RCC_APB1PeriphClockCmd(config->timerRcc, ENABLE);
	}

	GPIO_PinAFConfig(config->portA, config->pinSourceA, config->gpioAf);
	GPIO_PinAFConfig(config->portB, config->pinSourceB, config->gpioAf);

	gpioInitStructure.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_100MHz;

	gpioInitStructure.GPIO_Pin = config->pinA;
	GPIO_Init(config->portA, &gpioInitStructure);

	gpioInitStructure.GPIO_Pin = config->pinB;
	GPIO_Init(config->portB, &gpioInitStructure);

	TIM_DeInit(config->timer);

	TIM_TimeBaseStructInit(&timeBaseInitStructure);
	timeBaseInitStructure.TIM_Prescaler = 0U;
	timeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timeBaseInitStructure.TIM_Period = config->period;
	timeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timeBaseInitStructure.TIM_RepetitionCounter = 0U;
	TIM_TimeBaseInit(config->timer, &timeBaseInitStructure);

	TIM_EncoderInterfaceConfig(config->timer, TIM_EncoderMode_TI12,
							   TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

	TIM_ICStructInit(&icInitStructure);
	icInitStructure.TIM_ICFilter = BOARD_ENCODER_IC_FILTER;
	icInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInit(config->timer, &icInitStructure);
	icInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInit(config->timer, &icInitStructure);

	TIM_SetAutoreload(config->timer, config->period);
	TIM_SetCounter(config->timer, config->counterCenter);
	TIM_Cmd(config->timer, ENABLE);
}

static int32_t Encoder_ReadDelta(Encoder_Id_t encoder)
{
	const Encoder_Config_t *config;
	uint32_t currentCounter;
	int32_t delta;

	config = &g_encoderConfig[encoder];
	currentCounter = TIM_GetCounter(config->timer);

	if (config->is32Bit != 0U)
	{
		delta = (int32_t)(currentCounter - config->counterCenter);
	}
	else
	{
		delta = (int32_t)(int16_t)((uint16_t)currentCounter - (uint16_t)config->counterCenter);
	}

	TIM_SetCounter(config->timer, config->counterCenter);

	if (config->reverseFlag != 0U)
	{
		delta = -delta;
	}

	return delta;
}

void Encoder_Init(void)
{
	uint8_t i;

	for (i = 0U; i < ENCODER_COUNT; i++)
	{
		Encoder_InitOne((Encoder_Id_t)i);
	}

	Encoder_ClearAll();
}

void Encoder_UpdateSpeed(void)
{
	uint8_t i;
	int32_t delta;
	int32_t windowMs;
	int32_t oldDelta;
	int64_t denominator;

	for (i = 0U; i < ENCODER_COUNT; i++)
	{
		delta = Encoder_ReadDelta((Encoder_Id_t)i);
		g_encoderDeltaCount[i] = delta;
		g_encoderCount[i] += delta;

		oldDelta = 0;
		if (g_encoderValidSamples[i] >= BOARD_ENCODER_SPEED_AVG_SAMPLES)
		{
			oldDelta = g_encoderDeltaHistory[i][g_encoderHistoryIndex];
		}
		else
		{
			g_encoderValidSamples[i]++;
		}

		g_encoderDeltaHistory[i][g_encoderHistoryIndex] = delta;
		g_encoderDeltaWindowSum[i] += delta - oldDelta;
		windowMs = (int32_t)BOARD_ENCODER_SAMPLE_MS * (int32_t)g_encoderValidSamples[i];
		denominator = (int64_t)windowMs * (int64_t)BOARD_ENCODER_COUNTS_PER_OUTPUT_REV;
		g_encoderSpeedRpm[i] = Encoder_DivRoundClosest((int64_t)g_encoderDeltaWindowSum[i] * 60000LL,
													   denominator);
	}

	g_encoderHistoryIndex++;
	if (g_encoderHistoryIndex >= BOARD_ENCODER_SPEED_AVG_SAMPLES)
	{
		g_encoderHistoryIndex = 0U;
	}
}

int32_t Encoder_GetCount(Encoder_Id_t encoder)
{
	if (encoder >= ENCODER_COUNT)
	{
		return 0;
	}

	return g_encoderCount[encoder];
}

int32_t Encoder_GetDeltaCount(Encoder_Id_t encoder)
{
	if (encoder >= ENCODER_COUNT)
	{
		return 0;
	}

	return g_encoderDeltaCount[encoder];
}

int32_t Encoder_GetSpeedRpm(Encoder_Id_t encoder)
{
	if (encoder >= ENCODER_COUNT)
	{
		return 0;
	}

	return g_encoderSpeedRpm[encoder];
}

int32_t Encoder_GetLeftSpeedRpm(void)
{
	return (Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_LEFT_FRONT]) +
			Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_LEFT_REAR])) / 2;
}

int32_t Encoder_GetRightSpeedRpm(void)
{
	return (Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_RIGHT_FRONT]) +
			Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_RIGHT_REAR])) / 2;
}

void Encoder_Clear(Encoder_Id_t encoder)
{
	uint8_t historyIndex;

	if (encoder >= ENCODER_COUNT)
	{
		return;
	}

	g_encoderCount[encoder] = 0;
	g_encoderDeltaCount[encoder] = 0;
	g_encoderSpeedRpm[encoder] = 0;
	g_encoderDeltaWindowSum[encoder] = 0;
	g_encoderValidSamples[encoder] = 0U;
	for (historyIndex = 0U; historyIndex < BOARD_ENCODER_SPEED_AVG_SAMPLES; historyIndex++)
	{
		g_encoderDeltaHistory[encoder][historyIndex] = 0;
	}
	TIM_SetCounter(g_encoderConfig[encoder].timer, g_encoderConfig[encoder].counterCenter);
}

void Encoder_ClearAll(void)
{
	uint8_t i;

	g_encoderHistoryIndex = 0U;

	for (i = 0U; i < ENCODER_COUNT; i++)
	{
		Encoder_Clear((Encoder_Id_t)i);
	}
}
