#include "lvgl_port.h"
#include "ST7796.h"

/* 仍只占19200字节普通SRAM：内部先以RGB888（每色8位）绘图和混合透明边缘，
 * 全宽可放13行；送屏前原地压成RGB565。屏幕、SPI及DMA的像素格式仍是RGB565。
 * PARTIAL表示只保存当前小矩形，发送结束后其内容即可丢弃，不能用于整屏/DIRECT模式。 */
static uint8_t draw_buffer[19200U] __attribute__((aligned(4)));
static GUI_TouchReader touch_reader;
static void *touch_context;
static lv_indev_t *touch_device;
/* 松手或读失败时沿用最后有效位置，避免在释放事件中突然跳到(0,0)。 */
static lv_point_t last_touch_point;

/* 有序抖色：用固定的单像素亮度差分担不足一档的颜色误差，缓解RGB565渐变色带。
 * 阈值取屏幕绝对坐标，不能从每个flush的第0行重新开始，否则条带边缘会出现接缝。
 * 不使用随机数，静止画面、局部重绘和不同缓冲行数都会得到相同像素。 */
static const uint8_t bayer[8][8] = {
    {0,48,12,60,3,51,15,63},{32,16,44,28,35,19,47,31},
    {8,56,4,52,11,59,7,55},{40,24,36,20,43,27,39,23},
    {2,50,14,62,1,49,13,61},{34,18,46,30,33,17,45,29},
    {10,58,6,54,9,57,5,53},{42,26,38,22,41,25,37,21}
};
static unsigned channel_level(unsigned value, unsigned levels, unsigned threshold)
{
    /* levels为红/蓝31、绿色63。余数按阈值决定升一档；0和255始终保持端点。 */
    unsigned scaled = value * levels;
    return scaled / 255U + ((scaled % 255U) * 64U > threshold * 255U + 127U);
}
static void pack_rgb565(uint8_t *pixels, uint32_t source_stride, const lv_area_t *area)
{
    unsigned width = (unsigned)lv_area_get_width(area), height = (unsigned)lv_area_get_height(area);
    uint8_t *destination = pixels;
    for(unsigned y = 0; y < height; ++y) {
        const uint8_t *source = pixels + y * source_stride;
        for(unsigned x = 0; x < width; ++x) {
            /* LVGL的RGB888内存顺序为B、G、R。先取完整像素，再向低地址写两个字节；
             * 每行输出2*width小于输入步幅，因此不会覆盖尚未读取的源数据。 */
            unsigned b = source[0], g = source[1], r = source[2];
            unsigned threshold = bayer[(area->y1 + y) & 7U][(area->x1 + x) & 7U];
            uint16_t color = (uint16_t)((channel_level(r,31,threshold) << 11) |
                                       (channel_level(g,63,threshold) << 5) |
                                        channel_level(b,31,threshold));
            *destination++ = (uint8_t)color;
            *destination++ = (uint8_t)(color >> 8);
            source += 3;
        }
    }
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    lv_draw_buf_t *buffer = lv_display_get_buf_active(display);
    if(buffer && area->x1 >= 0 && area->y1 >= 0 && area->x2 < ST7796_WIDTH &&
       area->y2 < ST7796_HEIGHT && area->x1 <= area->x2 && area->y1 <= area->y2 &&
       buffer->header.cf == LV_COLOR_FORMAT_RGB888 &&
       buffer->header.stride >= (uint32_t)lv_area_get_width(area) * 3U) {
        /* 所有渐变、字体抗锯齿、图片缩放和透明阴影混合完成后，只在这里量化一次。
         * 原地压缩后行间不再保留RGB888的padding，传给LCD的是紧密排列的RGB565。 */
        pack_rgb565(pixels, buffer->header.stride, area);
        (void)ST7796_WriteRGB565((uint16_t)area->x1, (uint16_t)area->y1,
                    (uint16_t)lv_area_get_width(area), (uint16_t)lv_area_get_height(area),
                    pixels, (uint32_t)lv_area_get_width(area) * 2U);
    }
    /* 底层完成发送或安全停止失败传输后才返回，此时DMA已不再使用像素缓冲。
     * 无论成功还是出错，都要归还缓冲使用权，避免LVGL永远等待本次刷新。 */
    lv_display_flush_ready(display);
}
lv_display_t *GUI_DisplayInit(void)
{
    lv_display_t *display = lv_display_create(480, 320);
    if(!display) return NULL;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
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
