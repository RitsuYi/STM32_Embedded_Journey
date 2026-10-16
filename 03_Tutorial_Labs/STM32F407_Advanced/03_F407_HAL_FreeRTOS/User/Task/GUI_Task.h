#ifndef __GUI_TASK_H
#define __GUI_TASK_H
#include <stdint.h>
#include <stdbool.h>

#define GUI_TASK_STACK_WORDS 2048U
typedef struct {
    uint32_t ready, queue_dropped, stack_free_words, freertos_heap_free;
    uint32_t lvgl_heap_free, lvgl_heap_peak, lvgl_heap_fragmentation;
    const char *assert_file;
    int assert_line;
} GUI_Diagnostics;
extern volatile GUI_Diagnostics g_gui;

/* Called by main before task creation; LVGL APIs stay in the GUI task. */
bool GUI_TaskPrepare(void);
bool GUI_PostKey(unsigned key);

void vGUI_Task(void *pvParameters);

#endif
