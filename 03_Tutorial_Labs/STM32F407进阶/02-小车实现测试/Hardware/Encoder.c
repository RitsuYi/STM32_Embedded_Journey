/*
 * 编码器模块
 *
 * 这个文件负责：
 * 1. 初始化 4 路编码器对应的定时器与 GPIO 复用输入。
 * 2. 周期性读取每一路编码器在一个采样窗口内的增量计数。
 * 3. 维护累计脉冲数、单次增量、滑动窗口平均速度（RPM）。
 * 4. 按小车左右轮分组，提供左右侧平均转速。
 *
 * 这里采用“计数器居中”的读取方式：
 * - 每次读取前，编码器定时器的 CNT 都被放在一个中心值附近。
 * - 到下一个采样时刻，再读取 CNT 相对中心值偏移了多少，
 *   这个偏移量就是本采样周期内的脉冲增量。
 * - 读取完成后，再把 CNT 重新写回中心值，开始下一轮统计。
 *
 * 这样做的好处是：
 * - 对 16 位/32 位定时器都适用。
 * - 便于在短采样周期内稳定地得到“增量”而不是处理长时间累计值。
 * - 对定时器自然上溢/下溢更友好，尤其适合编码器接口模式。
 */
#include "Encoder.h"
#include "BoardConfig.h"

/* 每一路编码器的硬件描述表。
 * 所有硬件资源都通过 BoardConfig 宏注入，这样底层驱动不直接写死端口/引脚/定时器编号。 */
typedef struct
{
	/* 编码器接口所使用的定时器实例，例如 TIM2/TIM3/TIM4/TIM5。 */
	TIM_TypeDef *timer;
	/* 对应定时器的 RCC 时钟使能掩码。 */
	uint32_t timerRcc;
	/* 标记该定时器是否挂在 APB2，总线不同，对应的时钟使能函数也不同。 */
	uint8_t isApb2;
	/* 标记该定时器是否为 32 位计数器，用于区分 16 位/32 位差值计算方式。 */
	uint8_t is32Bit;
	/* 编码器安装方向可能与逻辑前进方向相反，置位后会在读数时统一取反。 */
	uint8_t reverseFlag;
	/* A/B 两相输入所在 GPIO 端口对应的 RCC 时钟掩码。 */
	uint32_t gpioRccA;
	uint32_t gpioRccB;
	/* A 相引脚信息。 */
	GPIO_TypeDef *portA;
	uint16_t pinA;
	uint8_t pinSourceA;
	/* B 相引脚信息。 */
	GPIO_TypeDef *portB;
	uint16_t pinB;
	uint8_t pinSourceB;
	/* GPIO 复用功能编号，需与目标定时器通道匹配。 */
	uint8_t gpioAf;
	/* 自动重装值，用于设置计数器上限。 */
	uint32_t period;
	/* 采样时用于“归中”的计数器中心值。 */
	uint32_t counterCenter;
} Encoder_Config_t;

/* 4 路编码器的静态硬件配置。
 * 顺序与 Encoder_Id_t 保持一致，外部通过枚举索引访问。 */
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

/* 累计总计数：从清零以来累计的脉冲数，可用于里程等统计。 */
static int32_t g_encoderCount[ENCODER_COUNT];
/* 单次采样增量：最近一个采样周期内的脉冲增量。 */
static int32_t g_encoderDeltaCount[ENCODER_COUNT];
/* 经过滑动窗口平均后的转速，单位 RPM。 */
static int32_t g_encoderSpeedRpm[ENCODER_COUNT];
/* 每一路编码器的历史增量窗口，用于降低测速抖动。 */
static int32_t g_encoderDeltaHistory[ENCODER_COUNT][BOARD_ENCODER_SPEED_AVG_SAMPLES];
/* 对应历史窗口的增量和，便于 O(1) 维护滑动平均。 */
static int32_t g_encoderDeltaWindowSum[ENCODER_COUNT];
/* 所有编码器共用的历史写入下标。每次更新速度后整体前进一格。 */
static uint8_t g_encoderHistoryIndex = 0U;
/* 当前每一路已经积累了多少个有效样本。
 * 在系统启动初期，样本数不足一个完整窗口时，平均值分母要用真实样本数。 */
static uint8_t g_encoderValidSamples[ENCODER_COUNT];

/* 小车轮子的位置定义。
 * 这里只定义“逻辑位置”，具体由哪一路编码器负责，交给下面的映射表决定。 */
typedef enum
{
	CAR_WHEEL_LEFT_FRONT = 0,
	CAR_WHEEL_RIGHT_FRONT,
	CAR_WHEEL_LEFT_REAR,
	CAR_WHEEL_RIGHT_REAR,
	CAR_WHEEL_COUNT
} Car_WheelPosition_t;

/* 小车轮位到编码器编号的映射表。
 * 这样上层拿“左前轮/右后轮”访问时，不必假设它一定对应 A/B/C/D 的某一个固定顺序。 */
static const Encoder_Id_t g_carWheelEncoderMap[CAR_WHEEL_COUNT] =
{
	(Encoder_Id_t)BOARD_CAR_LEFT_FRONT_ENCODER,
	(Encoder_Id_t)BOARD_CAR_RIGHT_FRONT_ENCODER,
	(Encoder_Id_t)BOARD_CAR_LEFT_REAR_ENCODER,
	(Encoder_Id_t)BOARD_CAR_RIGHT_REAR_ENCODER
};

/* 带四舍五入的整数除法。
 * 这里用于把“窗口内总脉冲数”换算成 RPM 时，尽量减少纯截断带来的误差。 */
static int32_t Encoder_DivRoundClosest(int64_t numerator, int64_t denominator)
{
	/* 分母非法时直接返回 0，避免除零。 */
	if (denominator <= 0)
	{
		return 0;
	}

	/* 正负数分别处理，保证四舍五入对称。 */
	if (numerator >= 0)
	{
		return (int32_t)((numerator + (denominator / 2)) / denominator);
	}

	return (int32_t)(-(((-numerator) + (denominator / 2)) / denominator));
}

/* 初始化单路编码器硬件。 */
static void Encoder_InitOne(Encoder_Id_t encoder)
{
	GPIO_InitTypeDef gpioInitStructure;
	TIM_TimeBaseInitTypeDef timeBaseInitStructure;
	TIM_ICInitTypeDef icInitStructure;
	const Encoder_Config_t *config;

	config = &g_encoderConfig[encoder];

	/* 先打开 GPIO 和定时器时钟。 */
	RCC_AHB1PeriphClockCmd(config->gpioRccA | config->gpioRccB, ENABLE);
	if (config->isApb2 != 0U)
	{
		RCC_APB2PeriphClockCmd(config->timerRcc, ENABLE);
	}
	else
	{
		RCC_APB1PeriphClockCmd(config->timerRcc, ENABLE);
	}

	/* 将 A/B 相引脚切到定时器复用功能。 */
	GPIO_PinAFConfig(config->portA, config->pinSourceA, config->gpioAf);
	GPIO_PinAFConfig(config->portB, config->pinSourceB, config->gpioAf);

	/* GPIO 配成复用推挽上拉输入，给编码器信号提供稳定输入环境。 */
	gpioInitStructure.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_100MHz;

	gpioInitStructure.GPIO_Pin = config->pinA;
	GPIO_Init(config->portA, &gpioInitStructure);

	gpioInitStructure.GPIO_Pin = config->pinB;
	GPIO_Init(config->portB, &gpioInitStructure);

	TIM_DeInit(config->timer);

	/* 定时器不分频，直接对编码器脉冲计数。 */
	TIM_TimeBaseStructInit(&timeBaseInitStructure);
	timeBaseInitStructure.TIM_Prescaler = 0U;
	timeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timeBaseInitStructure.TIM_Period = config->period;
	timeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timeBaseInitStructure.TIM_RepetitionCounter = 0U;
	TIM_TimeBaseInit(config->timer, &timeBaseInitStructure);

	/* 编码器接口模式：同时利用 TI1/TI2 两路输入，自动识别方向与增量。 */
	TIM_EncoderInterfaceConfig(config->timer, TIM_EncoderMode_TI12,
							   TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

	/* 给两个输入通道设置数字滤波，抑制毛刺。 */
	TIM_ICStructInit(&icInitStructure);
	icInitStructure.TIM_ICFilter = BOARD_ENCODER_IC_FILTER;
	icInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInit(config->timer, &icInitStructure);
	icInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInit(config->timer, &icInitStructure);

	/* 计数器初始化到中点位置，后续读增量时直接与这个中心值做差。 */
	TIM_SetAutoreload(config->timer, config->period);
	TIM_SetCounter(config->timer, config->counterCenter);
	TIM_Cmd(config->timer, ENABLE);
}

/* 读取单路编码器在一个采样周期内的增量计数。 */
static int32_t Encoder_ReadDelta(Encoder_Id_t encoder)
{
	const Encoder_Config_t *config;
	uint32_t currentCounter;
	int32_t delta;

	config = &g_encoderConfig[encoder];
	currentCounter = TIM_GetCounter(config->timer);

	/* 32 位定时器可以直接做带符号差值。
	 * 16 位定时器则需要先压到 16 位范围，再以 int16_t 解释，才能正确保留正负方向。 */
	if (config->is32Bit != 0U)
	{
		delta = (int32_t)(currentCounter - config->counterCenter);
	}
	else
	{
		delta = (int32_t)(int16_t)((uint16_t)currentCounter - (uint16_t)config->counterCenter);
	}

	/* 读完后立刻归中，为下一次采样准备。 */
	TIM_SetCounter(config->timer, config->counterCenter);

	/* 若机械安装方向与逻辑方向相反，则统一翻转符号。 */
	if (config->reverseFlag != 0U)
	{
		delta = -delta;
	}

	return delta;
}

void Encoder_Init(void)
{
	uint8_t i;

	/* 逐路初始化硬件，然后清空软件侧统计状态。 */
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
		/* 读出本周期增量，并维护累计总计数。 */
		delta = Encoder_ReadDelta((Encoder_Id_t)i);
		g_encoderDeltaCount[i] = delta;
		g_encoderCount[i] += delta;

		/* 滑动窗口已经满了，就把即将被覆盖的旧样本减掉；
		 * 如果还没满，则只增加有效样本计数。 */
		oldDelta = 0;
		if (g_encoderValidSamples[i] >= BOARD_ENCODER_SPEED_AVG_SAMPLES)
		{
			oldDelta = g_encoderDeltaHistory[i][g_encoderHistoryIndex];
		}
		else
		{
			g_encoderValidSamples[i]++;
		}

		/* 更新窗口内容与窗口总和。 */
		g_encoderDeltaHistory[i][g_encoderHistoryIndex] = delta;
		g_encoderDeltaWindowSum[i] += delta - oldDelta;

		/* 实际测速时间窗 = 采样周期 * 当前有效样本数。
		 * 上电早期样本数不足时，用真实样本数能让速度尽快收敛到合理值。 */
		windowMs = (int32_t)BOARD_ENCODER_SAMPLE_MS * (int32_t)g_encoderValidSamples[i];

		/* RPM 换算分母：
		 * 时间窗(ms) * 每输出轴一圈对应的编码器计数。 */
		denominator = (int64_t)windowMs * (int64_t)BOARD_ENCODER_COUNTS_PER_OUTPUT_REV;

		/* RPM = 窗口计数 * 60000 / (窗口时间ms * 每圈计数)。 */
		g_encoderSpeedRpm[i] = Encoder_DivRoundClosest((int64_t)g_encoderDeltaWindowSum[i] * 60000LL,
													   denominator);
	}

	/* 历史窗口指针循环前进。 */
	g_encoderHistoryIndex++;
	if (g_encoderHistoryIndex >= BOARD_ENCODER_SPEED_AVG_SAMPLES)
	{
		g_encoderHistoryIndex = 0U;
	}
}

int32_t Encoder_GetCount(Encoder_Id_t encoder)
{
	/* 越界保护，避免非法索引访问数组。 */
	if (encoder >= ENCODER_COUNT)
	{
		return 0;
	}

	return g_encoderCount[encoder];
}

int32_t Encoder_GetDeltaCount(Encoder_Id_t encoder)
{
	/* 返回最近一个采样周期内的增量计数。 */
	if (encoder >= ENCODER_COUNT)
	{
		return 0;
	}

	return g_encoderDeltaCount[encoder];
}

int32_t Encoder_GetSpeedRpm(Encoder_Id_t encoder)
{
	/* 返回经过滑动窗口平均后的转速值。 */
	if (encoder >= ENCODER_COUNT)
	{
		return 0;
	}

	return g_encoderSpeedRpm[encoder];
}

int32_t Encoder_GetLeftSpeedRpm(void)
{
	/* 左侧两轮速度取平均，用于整车层面的左右轮速度观察。 */
	return (Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_LEFT_FRONT]) +
			Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_LEFT_REAR])) / 2;
}

int32_t Encoder_GetRightSpeedRpm(void)
{
	/* 右侧两轮速度取平均。 */
	return (Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_RIGHT_FRONT]) +
			Encoder_GetSpeedRpm(g_carWheelEncoderMap[CAR_WHEEL_RIGHT_REAR])) / 2;
}

void Encoder_Clear(Encoder_Id_t encoder)
{
	uint8_t historyIndex;

	/* 非法编号直接忽略。 */
	if (encoder >= ENCODER_COUNT)
	{
		return;
	}

	/* 清空该路编码器的软件统计信息。 */
	g_encoderCount[encoder] = 0;
	g_encoderDeltaCount[encoder] = 0;
	g_encoderSpeedRpm[encoder] = 0;
	g_encoderDeltaWindowSum[encoder] = 0;
	g_encoderValidSamples[encoder] = 0U;
	for (historyIndex = 0U; historyIndex < BOARD_ENCODER_SPEED_AVG_SAMPLES; historyIndex++)
	{
		g_encoderDeltaHistory[encoder][historyIndex] = 0;
	}

	/* 同时把硬件计数器重新放回中心值。 */
	TIM_SetCounter(g_encoderConfig[encoder].timer, g_encoderConfig[encoder].counterCenter);
}

void Encoder_ClearAll(void)
{
	uint8_t i;

	/* 重置公共历史写指针，然后逐路清空。 */
	g_encoderHistoryIndex = 0U;

	for (i = 0U; i < ENCODER_COUNT; i++)
	{
		Encoder_Clear((Encoder_Id_t)i);
	}
}
