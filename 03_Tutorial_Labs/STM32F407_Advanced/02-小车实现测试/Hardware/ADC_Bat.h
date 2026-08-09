#ifndef __ADC_BAT_H
#define __ADC_BAT_H

#include "stm32f4xx.h"

void ADC_Bat_Init(void);
void ADC_Bat_SampleTask(void);
uint16_t ADC_Bat_GetRaw(void);
uint32_t ADC_Bat_GetVoltageMv(void);
uint16_t ADC_Bat_GetVoltageDeciV(void);

#endif
