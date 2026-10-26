#ifndef GUI_LVGL_PORT_H
#define GUI_LVGL_PORT_H
#include "lvgl.h"
#include <stdbool.h>

typedef bool (*GUI_TouchReader)(void *context, int32_t *x, int32_t *y, bool *pressed);
/* 输入定时器单独设为15ms，减少短按漏采；不改变显示刷新周期或芯片扫描率。
 * 这是调度目标，实际间隔还受GUI绘图、并口传输及任务调度影响。 */
#define GUI_TOUCH_READ_PERIOD_MS 15U

/* 每块32行RGB565，双缓冲共61440B，保留在普通SRAM。
 * 相比双20行多用23040B，减少历史滚动/切页时重复的绘图回调和窗口命令；
 * 每块30720B交DMA2写入FSMC，CPU同时画另一块；只处理变化区域，不强制刷新整屏。
 * 保留内存给通信/测量和运行时分配，不把剩余SRAM全部用作显示缓冲。
 * 行数保持1～32的原有内存预算，不借清理扩大缓冲。 */
#ifndef GUI_DRAW_BUFFER_ROWS
#define GUI_DRAW_BUFFER_ROWS 32U
#endif
#if GUI_DRAW_BUFFER_ROWS < 1 || GUI_DRAW_BUFFER_ROWS > 32
#error GUI_DRAW_BUFFER_ROWS_must_be_between_1_and_32
#endif
#define GUI_DRAW_BUFFER_BYTES (480U * GUI_DRAW_BUFFER_ROWS * 2U)
lv_display_t *GUI_DisplayInit(void);
/* 一帧指一次有像素发送、且全部传输成功收尾的LVGL刷新，可包含多个局部矩形。
 * fps_x10是最近约1秒的平均刷新次数乘10，例如125表示12.5 FPS；静止时可降为0。
 * 它不是液晶内部扫描率，也不是局部矩形块数；last_frame_ms包含布局、绘图和末块等待。 */
typedef struct {
    uint32_t fps_x10;
    uint32_t last_frame_ms;
    uint32_t max_frame_ms;
    uint32_t last_frame_blocks;
    uint32_t last_frame_bytes;
    uint32_t completed_frames;
    uint32_t transfer_errors;
    /* wait是GUI实际等待DMA归还缓冲的墙钟时间，单位ms，包含调度影响。
     * 与CPU绘图重叠的传输时间不重复计入wait，等待期间GUI通过信号量让出CPU。
     * pack字段保留为0以保持现有统计结构兼容；并口没有字节换序过程。
     * 这些数值不能直接推算CPU占用率。 */
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
