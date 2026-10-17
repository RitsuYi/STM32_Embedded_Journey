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

/* H6.5临时输入追踪：1显示LVGL光标，0关闭光标；事件计数始终保留供调试。
 * 光标仅帮助核对LVGL实际使用的位置，不能拦截按钮；排障后关闭此开关。 */
#define GUI_TOUCH_TRACE 0 /* 上下方向已定位并修正，默认不再创建临时触摸圆点。 */
typedef struct {
    uint32_t read_count, pressed_reports, released_reports, failed_reports;
    uint32_t pressed_events, released_events, clicked_events;
} GUI_TouchDiagnostics;
extern volatile GUI_TouchDiagnostics g_gui_touch;

lv_display_t *GUI_DisplayInit(void);
/* reader由GUI任务定时调用，返回已换算的480×320坐标及按下状态。
 * 硬件通信失败或坐标越界时按松手处理，并保留最后有效位置。
 * 先创建显示设备，再注册输入；本工程已接入FT6336U硬件读取。 */
lv_indev_t *GUI_RegisterTouch(GUI_TouchReader reader, void *context);
#endif
