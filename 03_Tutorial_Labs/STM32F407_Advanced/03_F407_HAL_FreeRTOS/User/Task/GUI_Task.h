#ifndef __GUI_TASK_H
#define __GUI_TASK_H
#include <stdint.h>
#include <stdbool.h>

#define GUI_TASK_STACK_WORDS 2048U

/* Called by main before task creation; LVGL APIs stay in the GUI task. */
bool GUI_TaskPrepare(void);
bool GUI_PostKey(unsigned key);

void vGUI_Task(void *pvParameters);

#endif
