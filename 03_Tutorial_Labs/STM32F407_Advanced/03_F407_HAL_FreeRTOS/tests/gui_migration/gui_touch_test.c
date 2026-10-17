#include "lvgl_port.h"
#include "gui_demo.h"
#include "ST7796.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

/* 此测试把真实lvgl_port和四页面应用交给LVGL 9.6运行，只替代SPI和触摸硬件。
 * 不直接发送CLICKED，只有按下/松手报文能够产生按钮事件，覆盖以前漏测的命中流程。 */
static uint32_t now_ms;
static int32_t touch_x, touch_y;
static bool pressed, read_valid = true;
static MeasurementPage page;
static InstrumentModel model;
static InstrumentUI ui;

uint32_t HAL_GetTick(void) { return now_ms; }
void GUI_LVGL_AssertFailed(const char *file, int line)
{
    fprintf(stderr, "LVGL assert: %s:%d\n", file, line);
    exit(1);
}
HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x, uint16_t y, uint16_t width,
    uint16_t height, const uint8_t *pixels, uint32_t stride)
{
    CHECK(x + width <= 480 && y + height <= 320 && pixels && stride >= width * 2U);
    return HAL_OK;
}
static bool reader(void *context, int32_t *x, int32_t *y, bool *down)
{
    (void)context;
    *x = touch_x; *y = touch_y; *down = pressed;
    return read_valid;
}
static void advance(uint32_t ms)
{
    /* 真实运行LVGL定时器；没有显式lv_indev_read来绕过设备定时器配置。 */
    for (uint32_t i = 0; i < ms; i += 5) {
        now_ms += 5;
        (void)lv_timer_handler();
    }
}
static void tap(lv_obj_t *button)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area;
    lv_obj_get_coords(button, &area);
    touch_x = (area.x1 + area.x2) / 2;
    touch_y = (area.y1 + area.y2) / 2;
    lv_point_t position = {touch_x, touch_y};
    lv_obj_t *hit = lv_indev_search_obj(lv_screen_active(), &position);
    printf("tap (%ld,%ld): hit=%p expected=%p\n", (long)touch_x, (long)touch_y,
           (void *)hit, (void *)button);
    CHECK(hit == button);
    pressed = true;
    advance(80);
    CHECK(lv_obj_has_state(button, LV_STATE_PRESSED));
    pressed = false;
    advance(80);
}
int main(void)
{
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_display_t *display = GUI_DisplayInit();
    CHECK(display);
    lv_indev_t *input = GUI_RegisterTouch(reader, NULL);
    CHECK(input && lv_indev_get_display(input) == display);
    instrument_defaults(&model);
    const MeasurementResult initial = {.geometry = MEASUREMENT_RING,
        .status = MEASUREMENT_PENDING, .connected = true, .simulated = true};
    measurement_page_create(&page, lv_screen_active(), &initial);
    GUI_DemoInit(&page, &model);
    const InstrumentServices services = {.capture_calibration = GUI_DemoCalibration};
    instrument_ui_create(&ui, &page, &model, &services, "模拟测量 / 仅内存");
    advance(100);

    /* 底部导航不依赖标定，必须逐个能由真实触摸状态切换。 */
    for (unsigned i = 1; i < 4; ++i) {
        tap(page.navigation[i]);
        CHECK(page.selected_navigation == i);
    }
    tap(page.navigation[0]);
    CHECK(page.selected_navigation == 0);
    CHECK(g_gui_touch.pressed_reports > 0 && g_gui_touch.failed_reports == 0);
    CHECK(g_gui_touch.pressed_events == 4 && g_gui_touch.released_events == 4);
    CHECK(g_gui_touch.clicked_events == 4);
    /* 手指在同一个宽按钮内轻微移动，不应让固定480×320屏幕开始滚动并吞掉点击。
     * 12像素仍在101像素宽的按钮内，但超过LVGL默认10像素滚动判定阈值。 */
    lv_area_t area;
    lv_obj_get_coords(page.navigation[1], &area);
    touch_x = (area.x1 + area.x2) / 2;
    touch_y = (area.y1 + area.y2) / 2;
    pressed = true;
    advance(80);
    touch_x += 12;
    advance(80);
    printf("within-button motion: scroll_obj=%p\n", (void *)lv_indev_get_scroll_obj(input));
    pressed = false;
    advance(80);
    CHECK(page.selected_navigation == 1);
    CHECK(g_gui_touch.clicked_events == 5);
    puts("PASS: pointer reports -> LVGL hit test -> four page navigation");
    lv_deinit();
    return 0;
}
