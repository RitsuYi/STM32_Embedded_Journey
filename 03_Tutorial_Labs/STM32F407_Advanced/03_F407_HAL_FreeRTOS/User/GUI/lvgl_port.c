#include "lvgl_port.h"
#include "ST7796.h"
#include "main.h"
#include "gui_lvgl_assert.h"

/* 两块480×32 RGB565局部缓冲共61440B，继续位于普通SRAM。
 * DMA读A时CPU可画B，传输结束后才归还A；CCM继续只放LVGL软件池。
 * 不增加像素中转缓冲，也不修改原缓冲容量和布局。 */
static uint8_t draw_buffer[2][GUI_DRAW_BUFFER_BYTES] __attribute__((aligned(4)));
static lv_display_t *lcd_display;
static bool retry_full;
static uint32_t retry_after;
static GUI_TouchReader touch_reader;
static void *touch_context;
static lv_indev_t *touch_device;
/* 松手保留最后有效点，避免释放事件跳到(0,0)。 */
static lv_point_t last_touch_point;

/* 只有GUI任务写统计；OLED任务在短临界区复制，不从别的任务访问LVGL对象。
 * 一次刷新可能分成多块传输，必须等整帧成功后才计1帧，不能把块数当FPS。 */
static GUI_DisplayStats display_stats;
static uint32_t rate_started_ms, rate_frames;
static uint32_t frame_started_ms, frame_blocks, frame_bytes;
static uint32_t frame_wait_ms;
static bool frame_tracking, frame_failed;
static bool flush_pending;
static uint32_t pending_bytes;
static void finish_flush(lv_display_t *display);

static void refresh_event(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_REFR_START) {
        /* 从布局/绘图之前计时，才能包含CPU绘图，而不只量最后一块并口发送。 */
        frame_started_ms = HAL_GetTick();
        frame_blocks = 0U;
        frame_bytes = 0U;
        frame_wait_ms=0U;
        frame_failed = false;
        frame_tracking = true;
    } else if(lv_event_get_code(event) == LV_EVENT_REFR_READY) {
        /* LVGL的REFR_READY本身不保证末块DMA已结束，先在GUI任务收尾。
         * 没有脏区的空刷新、任一块失败的刷新均不算成功帧。 */
        finish_flush(lv_event_get_target(event));
        if(frame_tracking && frame_blocks != 0U && !frame_failed) {
            display_stats.last_frame_ms = HAL_GetTick() - frame_started_ms;
            if(display_stats.last_frame_ms > display_stats.max_frame_ms)
                display_stats.max_frame_ms = display_stats.last_frame_ms;
            display_stats.last_frame_blocks = frame_blocks;
            display_stats.last_frame_bytes = frame_bytes;
            display_stats.last_frame_wait_ms=frame_wait_ms;
            display_stats.last_frame_pack_ms=0U;
            ++display_stats.completed_frames;
            ++rate_frames;
        }
        frame_tracking = false;
    }
}

void GUI_DisplayGetStats(GUI_DisplayStats *out)
{
    if(out) *out = display_stats;
}

static void failed_flush(void)
{
    ++display_stats.transfer_errors;
    frame_failed = true;
    /* 丢失的矩形不能靠下一次小数字重绘修复；安全任务阶段限速请求完整重绘。
     * 错误不在刷新回调内直接invalidate，避免本轮失效列表被递归修改。 */
    retry_full=true;retry_after=HAL_GetTick()+100U;
}
static void finish_flush(lv_display_t *display)
{
    if(!flush_pending) return;
    uint32_t began = HAL_GetTick();
    HAL_StatusTypeDef status = ST7796_WriteRGB565Wait();
    /* HAL_BUSY表示调用任务/交接顺序违反约定，DMA仍占有源；此时禁止归还。
     * 正常超时/硬件错误由驱动安全停流后返回，不走这条程序错误保护。 */
    if(status == HAL_BUSY) { GUI_LVGL_AssertFailed(__FILE__, __LINE__); for(;;) { } }
    frame_wait_ms += HAL_GetTick() - began;
    if(status != HAL_OK) failed_flush();
    else {
        ++frame_blocks;
        frame_bytes += pending_bytes;
    }
    /* 此函数只在GUI任务运行，ISR从不调用LVGL。驱动已确认DMA不再读源，
     * 现在才可flush_ready；同一块缓冲从Start到这里一直受到保护。 */
    flush_pending = false;
    lv_display_flush_ready(display);
}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{
    lv_draw_buf_t *buffer=lv_display_get_buf_active(display);
    uint32_t width=(uint32_t)lv_area_get_width(area),height=(uint32_t)lv_area_get_height(area);
    if(!buffer || area->x1<0 || area->y1<0 || area->x2>=ST7796_WIDTH || area->y2>=ST7796_HEIGHT ||
       area->x1>area->x2 || area->y1>area->y2 || buffer->header.cf!=LV_COLOR_FORMAT_RGB565 ||
       buffer->header.stride<width*2U) {
        failed_flush();lv_display_flush_ready(display);return;
    }
    /* 并口使用原生16位RGB565，不修改/紧密化源缓冲。
     * stride原样交给驱动，窄矩形行尾padding不会被误当成像素。
     * Start只设置窗口和启动DMA，返回后LVGL可以绘制另一块缓冲。 */
    HAL_StatusTypeDef status = ST7796_WriteRGB565Start((uint16_t)area->x1, (uint16_t)area->y1,
                                  (uint16_t)width, (uint16_t)height,
                                  pixels, buffer->header.stride);
    /* LVGL本应先等待前一块，若仍忙则保留缓冲并停机，不能假装发送结束。 */
    if(status == HAL_BUSY) { GUI_LVGL_AssertFailed(__FILE__, __LINE__); for(;;) { } }
    if(status != HAL_OK) {
        failed_flush();
        lv_display_flush_ready(display);
        return;
    }
    pending_bytes = width * height * 2U;
    flush_pending = true;
    /* 中间块交给LVGL的flush_wait回调等待；末块主动收尾，保证整帧统计
     * 和本轮GUI任务结束时没有悬空传输，同时不阻碍前面各块的绘制重叠。 */
    if(lv_display_flush_is_last(display)) finish_flush(display);
}
lv_display_t *GUI_DisplayInit(void)
{
    if(ST7796_DMAInit() != HAL_OK) return NULL;
    lv_display_t *display=lv_display_create(ST7796_WIDTH,ST7796_HEIGHT);
    if(!display) return NULL;
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,draw_buffer[0],draw_buffer[1],sizeof(draw_buffer[0]),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_display_set_flush_wait_cb(display,finish_flush);
    /* 使用本地9.6的刷新起止事件，不启用LCD上的LVGL性能浮窗。 */
    lv_display_add_event_cb(display,refresh_event,LV_EVENT_REFR_START,NULL);
    lv_display_add_event_cb(display,refresh_event,LV_EVENT_REFR_READY,NULL);
    display_stats = (GUI_DisplayStats){0};
    rate_started_ms = HAL_GetTick();
    rate_frames = 0U;
    lcd_display=display;return display;
}
void GUI_DisplayService(void)
{
    /* 放在lv_timer_handler之后调用；只在失败后修复LCD可能遗漏的区域，正常帧无额外重绘。 */
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - rate_started_ms;
    if(elapsed >= 1000U) {
        /* 按真实经过的毫秒数计算，不能假设GUI每次恰好1秒被调度；无符号差值兼容回绕。
         * 乘10000是把“每毫秒帧数”换为“每秒帧数×10”，64位中间值避免乘法溢出。 */
        display_stats.fps_x10 = (uint32_t)(((uint64_t)rate_frames * 10000U + elapsed / 2U) / elapsed);
        rate_frames = 0U;
        rate_started_ms = now;
    }
    if(retry_full && (int32_t)(HAL_GetTick()-retry_after)>=0) {
        retry_full=false;lv_obj_invalidate(lv_display_get_screen_active(lcd_display));
    }
}
static void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    int32_t x = 0, y = 0;
    bool pressed = false;
    data->state = LV_INDEV_STATE_RELEASED;
    data->point = last_touch_point;
    data->continue_reading = false;
    /* 驱动已按商家CONTACT规则判定有效性；GUI只转交状态和坐标。
     * 每个输入周期读一次，不加二次确认、历史滤波、补交按键或边缘偏移。 */
    bool valid = touch_reader && touch_reader(touch_context, &x, &y, &pressed);
    if (valid && pressed && x >= 0 && x < ST7796_WIDTH && y >= 0 && y < ST7796_HEIGHT) {
        data->point = (lv_point_t){x, y};
        data->state = LV_INDEV_STATE_PRESSED;
    }
    if (data->state == LV_INDEV_STATE_PRESSED) last_touch_point = data->point;
}
lv_indev_t *GUI_RegisterTouch(GUI_TouchReader reader, void *context)
{
    touch_reader = reader; touch_context = context;
    if(!reader) return NULL;
    if(!touch_device) {
        touch_device = lv_indev_create();
        if(!touch_device) return NULL;
        lv_indev_set_type(touch_device, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_device, touch_read);
        /* 输入以15ms为目标采样，显示仍保持原周期；不再叠加滤波或等待确认。
         * 同一GUI任务串行读触摸和绘图，实际间隔仍受并口刷新及总线等待影响。 */
        lv_indev_set_display(touch_device, lv_display_get_default());
        lv_timer_set_period(lv_indev_get_read_timer(touch_device), GUI_TOUCH_READ_PERIOD_MS);
    }
    return touch_device;
}
