#ifndef __OLED_TASK_H
#define __OLED_TASK_H

#include "OLED.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

/* 新增共用总线恢复会嵌套调用HAL/GPIO和RTOS延时，留出512B额外栈空间。
 * 单位是32位字，256字=1024B；不能按字节误传给xTaskCreate。 */
#define OLED_TASK_STACK_WORDS 256U

void vOLED_Task(void *pvParameters);

#endif
