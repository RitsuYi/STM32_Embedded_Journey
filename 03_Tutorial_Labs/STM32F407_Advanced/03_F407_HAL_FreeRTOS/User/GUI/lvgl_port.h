#ifndef GUI_LVGL_PORT_H
#define GUI_LVGL_PORT_H
#include "lvgl.h"
#include <stdbool.h>

typedef bool (*GUI_TouchReader)(void *context, int32_t *x, int32_t *y, bool *pressed);
/* 输入定时器单独设为15ms，减少短按漏采；不改变显示刷新周期或芯片扫描率。
 * 这是调度目标，实际间隔还受GUI绘图、SPI传输及任务调度影响。 */
#define GUI_TOUCH_READ_PERIOD_MS 15U

/* 每块32行RGB565，双缓冲共61440B，只使用DMA可读的普通SRAM。
 * 相比双20行多用23040B，减少历史滚动/切页时重复的绘图回调和窗口命令；
 * 每块30720B仍可一次DMA发送。缓冲只处理LVGL标记的变化区域，不强制刷新整屏。
 * 保留内存给通信/测量和运行时分配，不把剩余SRAM全部用作显示缓冲。
 * 主机对照可覆盖行数；每块最多32行，确保一次发送小于65535字节。 */
#ifndef GUI_DRAW_BUFFER_ROWS
#define GUI_DRAW_BUFFER_ROWS 32U
#endif
#if GUI_DRAW_BUFFER_ROWS < 1 || GUI_DRAW_BUFFER_ROWS > 32
#error GUI_DRAW_BUFFER_ROWS_must_be_between_1_and_32
#endif
#define GUI_DRAW_BUFFER_BYTES (480U * GUI_DRAW_BUFFER_ROWS * 2U)
lv_display_t *GUI_DisplayInit(void);
/* 一帧指一次有像素发送、且全部DMA成功收尾的LVGL刷新，可包含多个局部矩形。
 * fps_x10是最近约1秒的平均刷新次数乘10，例如125表示12.5 FPS；静止时可降为0。
 * 它不是液晶内部扫描率，也不是DMA块数；last_frame_ms包含布局、绘图和末块等待。 */
typedef struct {
    uint32_t fps_x10;
    uint32_t last_frame_ms;
    uint32_t max_frame_ms;
    uint32_t last_frame_blocks;
    uint32_t last_frame_bytes;
    uint32_t completed_frames;
    uint32_t transfer_errors;
    /* 以下两项供调试器分项观察，OLED原三行保持：wait是Finish等待的墙钟时间，
     * pack是字节换序/紧密化的墙钟时间，单位ms。它们不是CPU周期或DMA总忙时长；
     * DMA与绘图重叠、任务抢占和毫秒分辨率都会影响数值，不可直接相加推算CPU占用。 */
    uint32_t last_frame_wait_ms;
    uint32_t last_frame_pack_ms;
} GUI_DisplayStats;
/* 只复制普通统计数据，不调用LVGL。跨任务读取时，调用方须用短临界区包住本函数，
 * 防止复制到一半被GUI任务抢占；临界区外再排版和发送OLED，禁止持锁做I²C传输。 */
void GUI_DisplayGetStats(GUI_DisplayStats *out);
/* GUI任务每轮LVGL处理后调用，及时收尾并在传输失败后请求完整恢复重绘。 */
void GUI_DisplayService(void);
/* reader由GUI任务定时调用，返回已换算的480×320坐标及按下状态。
 * 硬件通信失败或坐标越界时按松手处理，并保留最后有效位置。
 * 先创建显示设备，再注册输入；本工程已接入FT6336U硬件读取。 */
lv_indev_t *GUI_RegisterTouch(GUI_TouchReader reader, void *context);
#endif
