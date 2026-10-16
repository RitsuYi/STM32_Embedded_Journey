#ifndef GUI_LVGL_PORT_H
#define GUI_LVGL_PORT_H
#include "lvgl.h"
#include <stdbool.h>

typedef bool (*GUI_TouchReader)(void *context, int32_t *x, int32_t *y, bool *pressed);
typedef struct {
    uint32_t flush_count, flush_errors;
    uint32_t last_spi_status, last_flush_ms, max_flush_ms;
} GUI_DisplayDiagnostics;
extern volatile GUI_DisplayDiagnostics g_gui_display;

lv_display_t *GUI_DisplayInit(void);
/* Reader must supply already rotated 480x320 coordinates; called only by GUI.
 * Register only after the FT6336U bus/reset wiring has been confirmed. */
lv_indev_t *GUI_RegisterTouch(GUI_TouchReader reader, void *context);
#endif
