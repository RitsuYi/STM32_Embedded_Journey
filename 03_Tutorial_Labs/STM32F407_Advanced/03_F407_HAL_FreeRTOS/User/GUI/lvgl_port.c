#include "lvgl_port.h"
#include "ST7796.h"

static uint8_t draw_buffer[480U * 20U * 2U] __attribute__((aligned(4)));
static GUI_TouchReader touch_reader;
static void *touch_context;
static lv_indev_t *touch_device;
/* 松手或读失败时沿用最后有效位置，避免在释放事件中突然跳到(0,0)。 */
static lv_point_t last_touch_point;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    lv_draw_buf_t *buffer = lv_display_get_buf_active(display);
    if(buffer && area->x1 >= 0 && area->y1 >= 0 && area->x2 < ST7796_WIDTH &&
       area->y2 < ST7796_HEIGHT && area->x1 <= area->x2 && area->y1 <= area->y2) {
        (void)ST7796_WriteRGB565((uint16_t)area->x1, (uint16_t)area->y1,
                    (uint16_t)lv_area_get_width(area), (uint16_t)lv_area_get_height(area),
                    pixels, buffer->header.stride);
    }
    /* 底层完成发送或安全停止失败传输后才返回，此时DMA已不再使用像素缓冲。
     * 无论成功还是出错，都要归还缓冲使用权，避免LVGL永远等待本次刷新。 */
    lv_display_flush_ready(display);
}
lv_display_t *GUI_DisplayInit(void)
{
    lv_display_t *display = lv_display_create(480, 320);
    if(!display) return NULL;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    return display;
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
         * 同一GUI任务串行读触摸和绘图，实际间隔仍受SPI刷新及总线等待影响。 */
        lv_indev_set_display(touch_device, lv_display_get_default());
        lv_timer_set_period(lv_indev_get_read_timer(touch_device), GUI_TOUCH_READ_PERIOD_MS);
    }
    return touch_device;
}
