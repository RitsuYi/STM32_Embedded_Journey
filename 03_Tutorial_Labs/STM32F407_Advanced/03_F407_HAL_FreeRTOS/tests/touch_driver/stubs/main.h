#ifndef TOUCH_TEST_MAIN_H
#define TOUCH_TEST_MAIN_H
#include "stm32f4xx_hal.h"
extern GPIO_TypeDef test_port;
#define TP_RST_GPIO_Port (&test_port)
#define TP_INT_GPIO_Port (&test_port)
#define TP_RST_Pin 1U
#define TP_INT_Pin 2U
#endif
