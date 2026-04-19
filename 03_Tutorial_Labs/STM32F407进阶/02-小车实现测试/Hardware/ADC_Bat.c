#include "ADC_Bat.h"
#include "BoardConfig.h"

static uint16_t g_batSamples[BOARD_BAT_ADC_FILTER_SIZE];
static uint32_t g_batSampleSum = 0U;
static uint16_t g_batFilteredRaw = 0U;
static uint8_t g_batSampleIndex = 0U;

static uint16_t ADC_Bat_ReadRawOnce(void)
{
	uint32_t timeout;

	ADC_RegularChannelConfig(BOARD_BAT_ADC, BOARD_BAT_ADC_CHANNEL, 1U, BOARD_BAT_ADC_SAMPLE_TIME);
	ADC_SoftwareStartConv(BOARD_BAT_ADC);

	timeout = 0xFFFFU;
	while (ADC_GetFlagStatus(BOARD_BAT_ADC, ADC_FLAG_EOC) == RESET)
	{
		if (timeout-- == 0U)
		{
			return g_batFilteredRaw;
		}
	}

	return (uint16_t)ADC_GetConversionValue(BOARD_BAT_ADC);
}

static void ADC_Bat_FillFilter(uint16_t raw)
{
	uint8_t i;

	g_batSampleSum = 0U;
	for (i = 0U; i < BOARD_BAT_ADC_FILTER_SIZE; i++)
	{
		g_batSamples[i] = raw;
		g_batSampleSum += raw;
	}

	g_batFilteredRaw = raw;
	g_batSampleIndex = 0U;
}

void ADC_Bat_Init(void)
{
	GPIO_InitTypeDef gpioInitStructure;
	ADC_CommonInitTypeDef adcCommonInitStructure;
	ADC_InitTypeDef adcInitStructure;
	uint16_t raw;

	RCC_AHB1PeriphClockCmd(BOARD_BAT_ADC_GPIO_RCC, ENABLE);
	RCC_APB2PeriphClockCmd(BOARD_BAT_ADC_RCC, ENABLE);

	gpioInitStructure.GPIO_Pin = BOARD_BAT_ADC_PIN;
	gpioInitStructure.GPIO_Mode = GPIO_Mode_AN;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_BAT_ADC_PORT, &gpioInitStructure);

	ADC_DeInit();

	ADC_CommonStructInit(&adcCommonInitStructure);
	adcCommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	adcCommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
	adcCommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	adcCommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&adcCommonInitStructure);

	ADC_StructInit(&adcInitStructure);
	adcInitStructure.ADC_Resolution = ADC_Resolution_12b;
	adcInitStructure.ADC_ScanConvMode = DISABLE;
	adcInitStructure.ADC_ContinuousConvMode = DISABLE;
	adcInitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	adcInitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	adcInitStructure.ADC_NbrOfConversion = 1U;
	ADC_Init(BOARD_BAT_ADC, &adcInitStructure);

	ADC_Cmd(BOARD_BAT_ADC, ENABLE);

	raw = ADC_Bat_ReadRawOnce();
	ADC_Bat_FillFilter(raw);
}

void ADC_Bat_SampleTask(void)
{
	uint16_t raw;

	raw = ADC_Bat_ReadRawOnce();

	g_batSampleSum -= g_batSamples[g_batSampleIndex];
	g_batSamples[g_batSampleIndex] = raw;
	g_batSampleSum += raw;

	g_batSampleIndex++;
	if (g_batSampleIndex >= BOARD_BAT_ADC_FILTER_SIZE)
	{
		g_batSampleIndex = 0U;
	}

	g_batFilteredRaw = (uint16_t)(g_batSampleSum / BOARD_BAT_ADC_FILTER_SIZE);
}

uint16_t ADC_Bat_GetRaw(void)
{
	return g_batFilteredRaw;
}

uint32_t ADC_Bat_GetVoltageMv(void)
{
	uint32_t adcMv;
	uint32_t batteryMv;

	adcMv = ((uint32_t)g_batFilteredRaw * BOARD_ADC_REFERENCE_MV) / 4095U;
	batteryMv = (adcMv * BOARD_BAT_DIVIDER_NUMERATOR) / BOARD_BAT_DIVIDER_DENOMINATOR;
	batteryMv += BOARD_BAT_VOLTAGE_OFFSET_MV;

	return batteryMv;
}

uint16_t ADC_Bat_GetVoltageDeciV(void)
{
	return (uint16_t)((ADC_Bat_GetVoltageMv() + 50U) / 100U);
}
