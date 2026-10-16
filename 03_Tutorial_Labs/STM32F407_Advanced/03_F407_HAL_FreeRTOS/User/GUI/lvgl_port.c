#include "lvgl_port.h"
#include "ST7796.h"
#include "main.h"

static uint8_t draw_buffer[480U * 20U * 2U] __attribute__((aligned(4)));
static GUI_TouchReader touch_reader;
static void *touch_context;
static lv_indev_t *touch_device;
volatile GUI_DisplayDiagnostics g_gui_display;

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
    if(touch_reader && touch_reader(touch_context, &x, &y, &pressed) && pressed &&
       x >= 0 && x < 480 && y >= 0 && y < 320) {
        data->point.x = x; data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
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
    }
    return touch_device;
}
