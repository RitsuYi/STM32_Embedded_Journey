#ifndef GUI_LVGL_PORT_H
#define GUI_LVGL_PORT_H
#include "lvgl.h"
#include <stdbool.h>

typedef bool (*GUI_TouchReader)(void *context, int32_t *x, int32_t *y, bool *pressed);
/* 输入定时器单独设为15ms，减少短按漏采；不改变显示刷新周期或芯片扫描率。
 * 这是调度目标，实际间隔还受GUI绘图、SPI传输及任务调度影响。 */
#define GUI_TOUCH_READ_PERIOD_MS 15U

/* 创建单PARTIAL显示：19200B内部RGB888绘图，flush统一抖成RGB565后同步发送。
 * 返回显示句柄，创建失败返回NULL；本接口不能改成DIRECT而复用原地压缩流程。 */
lv_display_t *GUI_DisplayInit(void);
/* reader由GUI任务定时调用，返回已换算的480×320坐标及按下状态。
 * 硬件通信失败或坐标越界时按松手处理，并保留最后有效位置。
 * 先创建显示设备，再注册输入；本工程已接入FT6336U硬件读取。 */
lv_indev_t *GUI_RegisterTouch(GUI_TouchReader reader, void *context);
#endif
