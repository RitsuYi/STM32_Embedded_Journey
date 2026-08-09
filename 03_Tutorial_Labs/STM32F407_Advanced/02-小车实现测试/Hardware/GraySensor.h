#ifndef __GRAY_SENSOR_H
#define __GRAY_SENSOR_H

#include "stm32f4xx.h"

typedef enum
{
	GRAY_SENSOR_LEFT_2 = 0,
	GRAY_SENSOR_LEFT_1,
	GRAY_SENSOR_CENTER,
	GRAY_SENSOR_RIGHT_1,
	GRAY_SENSOR_RIGHT_2,
	GRAY_SENSOR_COUNT
} GraySensor_Position_t;

#define GRAY_SENSOR_MASK_LEFT_2                  ((uint8_t)(1U << GRAY_SENSOR_LEFT_2))
#define GRAY_SENSOR_MASK_LEFT_1                  ((uint8_t)(1U << GRAY_SENSOR_LEFT_1))
#define GRAY_SENSOR_MASK_CENTER                  ((uint8_t)(1U << GRAY_SENSOR_CENTER))
#define GRAY_SENSOR_MASK_RIGHT_1                 ((uint8_t)(1U << GRAY_SENSOR_RIGHT_1))
#define GRAY_SENSOR_MASK_RIGHT_2                 ((uint8_t)(1U << GRAY_SENSOR_RIGHT_2))

#define GRAY_SENSOR_LINE_POSITION_NONE           ((int16_t)32767)

void GraySensor_Init(void);
uint8_t GraySensor_ReadRaw(GraySensor_Position_t position);
uint8_t GraySensor_Read(GraySensor_Position_t position);
uint8_t GraySensor_GetRawMask(void);
uint8_t GraySensor_GetMask(void);
uint8_t GraySensor_HasLine(void);
int16_t GraySensor_GetLinePosition(void);

#endif
