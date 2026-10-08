#ifndef __OLED_TASK_H
#define __OLED_TASK_H

#include "OLED.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

void vOLED_Task(void *pvParameters);

#endif
