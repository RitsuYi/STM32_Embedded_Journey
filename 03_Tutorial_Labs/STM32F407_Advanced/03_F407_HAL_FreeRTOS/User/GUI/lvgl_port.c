#include "lvgl_port.h"
#include "ST7796.h"
#include "main.h"

static uint8_t draw_buffer[480U * 20U * 2U] __attribute__((aligned(4)));
static GUI_TouchReader touch_reader;
static void *touch_context;
static lv_indev_t *touch_device;
/* 松手或读失败时沿用最后有效位置，避免在释放事件中突然跳到(0,0)。 */
static lv_point_t last_touch_point;
volatile GUI_DisplayDiagnostics g_gui_display;
volatile GUI_TouchDiagnostics g_gui_touch;
#if GUI_TOUCH_TRACE
static lv_obj_t *touch_cursor;
#endif

/* 只观察LVGL输入设备已经产生的事件，不手工发送点击、不改变事件处理。
 * PRESSED/RELEASED/CLICKED分别表示LVGL认定按下/松手/完成点击，
 * 与FT6336U的P字段不同：P只说明驱动接受了这一帧坐标。 */
static void trace_touch_event(lv_event_t *event)
{
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED: ++g_gui_touch.pressed_events; break;
        case LV_EVENT_RELEASED: ++g_gui_touch.released_events; break;
        case LV_EVENT_CLICKED: ++g_gui_touch.clicked_events; break;
        default: break;
    }
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint32_t started = HAL_GetTick();
    HAL_StatusTypeDef status = HAL_ERROR;
    lv_draw_buf_t *buffer = lv_display_get_buf_active(display);
    if(buffer && area->x1 >= 0 && area->y1 >= 0 && area->x2 < ST7796_WIDTH &&
       area->y2 < ST7796_HEIGHT && area->x1 <= area->x2 && area->y1 <= area->y2) {
        status = ST7796_WriteRGB565((uint16_t)area->x1, (uint16_t)area->y1,
                    (uint16_t)lv_area_get_width(area), (uint16_t)lv_area_get_height(area),
                    pixels, buffer->header.stride);
    }
    g_gui_display.last_spi_status = status;
    ++g_gui_display.flush_count;
    if(status != HAL_OK) ++g_gui_display.flush_errors;
    g_gui_display.last_flush_ms = HAL_GetTick() - started;
    if(g_gui_display.last_flush_ms > g_gui_display.max_flush_ms)
        g_gui_display.max_flush_ms = g_gui_display.last_flush_ms;
    /* Blocking transfer is finished or has returned an error. No outstanding
     * DMA still owns pixels. Error counts remain visible to the debugger. */
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
    ++g_gui_touch.read_count;
    bool valid = touch_reader && touch_reader(touch_context, &x, &y, &pressed);
    if (!valid) ++g_gui_touch.failed_reports;
    if(valid && pressed &&
       x >= 0 && x < 480 && y >= 0 && y < 320) {
        data->point.x = x; data->point.y = y;
        last_touch_point = data->point;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    if (data->state == LV_INDEV_STATE_PRESSED) ++g_gui_touch.pressed_reports;
    else ++g_gui_touch.released_reports;
#if GUI_TOUCH_TRACE
    /* 光标由lv_indev_set_cursor绑定，位置由LVGL输入处理本身移动。
     * 此处只控制按下时显示/松手时隐藏，便于核对位置和完整按下-松手过程。 */
    if (touch_cursor) lv_obj_set_hidden(touch_cursor, data->state != LV_INDEV_STATE_PRESSED);
#endif
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
        /* 显式绑定已创建的显示设备，读取定时器仍沿用本地LVGL默认33ms。
         * 事件监听只采证，不用增大点击区域或直接发送CLICKED掩盖输入问题。 */
        lv_indev_set_display(touch_device, lv_display_get_default());
        if (!lv_indev_add_event_cb(touch_device, trace_touch_event, LV_EVENT_ALL, NULL)) return NULL;
#if GUI_TOUCH_TRACE
        touch_cursor = lv_obj_create(lv_display_get_layer_sys(lv_indev_get_display(touch_device)));
        if (!touch_cursor) return NULL;
        lv_obj_remove_style_all(touch_cursor);
        lv_obj_set_size(touch_cursor, 10, 10);
        lv_obj_set_style_radius(touch_cursor, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(touch_cursor, lv_color_hex(0x00FFFF), 0);
        lv_obj_set_style_bg_opa(touch_cursor, LV_OPA_COVER, 0);
        lv_obj_set_style_translate_x(touch_cursor, -5, 0);
        lv_obj_set_style_translate_y(touch_cursor, -5, 0);
        lv_obj_set_scrollable(touch_cursor, false);
        lv_obj_set_hidden(touch_cursor, true);
        /* LVGL在绑定时会清除光标的可点击属性，圆点不会挡住下面的按钮。 */
        lv_indev_set_cursor(touch_device, touch_cursor);
#endif
    }
    return touch_device;
}
