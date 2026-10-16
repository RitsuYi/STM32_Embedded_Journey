#include "gui_app.h"
#include "gui_demo.h"
#include "fonts/measurement_fonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if(!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while(0)
static uint32_t now_ms, flushes;
static uint8_t buffer[480U * 20U * 2U] __attribute__((aligned(4)));
static MeasurementPage page;
static InstrumentModel model;
static InstrumentUI ui;
static uint32_t now(void) { return now_ms; }
void GUI_LVGL_AssertFailed(const char *file, int line)
{
    fprintf(stderr, "LVGL assertion: %s:%d\n", file, line); exit(1);
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    CHECK(area->x1 >= 0 && area->x2 < 480 && area->y1 >= 0 && area->y2 < 320);
    CHECK(area->x1 <= area->x2 && area->y1 <= area->y2 && pixels);
    lv_draw_buf_t *active = lv_display_get_buf_active(display);
    CHECK(active && active->header.stride >= (uint32_t)lv_area_get_width(area) * 2U);
    CHECK((uint32_t)lv_area_get_height(area) * active->header.stride <= sizeof(buffer));
    ++flushes; lv_display_flush_ready(display);
}
static lv_display_t *display_init(void)
{
    lv_init(); lv_tick_set_cb(now);
    lv_display_t *display = lv_display_create(480, 320); CHECK(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    return display;
}
static void advance(lv_display_t *display, uint32_t milliseconds)
{
    now_ms += milliseconds; lv_timer_handler(); lv_refr_now(display);
    CHECK(lv_mem_test() == LV_RESULT_OK);
}
static void click(lv_obj_t *object)
{
    CHECK(object && !lv_obj_is_hidden(object));
    CHECK(!lv_obj_has_state(object, LV_STATE_DISABLED));
    lv_obj_send_event(object, LV_EVENT_CLICKED, NULL);
}
static bool fail_save(void *context, const InstrumentModel *value)
{
    (void)context; (void)value; return false;
}
int main(void)
{
    lv_display_t *display = display_init();
    const char *text = "模拟测量仅内存不保存未接入操作已更新当前不支持背光调节";
    const lv_font_t *fonts[] = {&measurement_font_12, &measurement_font_14, &measurement_font_16_bold};
    for(const unsigned char *s = (const unsigned char *)text; *s; s += 3) {
        uint32_t cp = ((uint32_t)(s[0] & 15) << 12) | ((uint32_t)(s[1] & 63) << 6) | (s[2] & 63);
        for(unsigned i = 0; i < 3; ++i) {
            lv_font_glyph_dsc_t glyph;
            CHECK(lv_font_get_glyph_dsc(fonts[i], &glyph, cp, 0) && !glyph.is_placeholder);
        }
    }
    GUI_AppInit(NULL);
    for(unsigned i = 0; i < 12; ++i) { GUI_AppKey(2); advance(display, 50); }
    lv_deinit();

    display = display_init();
    instrument_defaults(&model);
    const MeasurementResult initial = {.geometry = MEASUREMENT_RING, .status = MEASUREMENT_PENDING,
        .connected = true, .simulated = true};
    measurement_page_create(&page, lv_screen_active(), &initial);
    GUI_DemoInit(&page, &model);
    const InstrumentServices services = {.capture_calibration = GUI_DemoCalibration, .persistent = false};
    instrument_ui_create(&ui, &page, &model, &services, "模拟测量 / 仅内存");
    advance(display, 50);
    CHECK(!measurement_page_start(&page)); /* no calibration must block measurement */
    measurement_page_navigate(&page, 1);
    click(ui.actions[0]); click(ui.actions[0]); click(ui.actions[0]);
    advance(display, 1500);
    CHECK(ui.calibration_step == 2 && ui.candidate.valid);
    click(ui.actions[0]);
    CHECK(instrument_calibration_ready(&model) && model.calibration.simulated);
    CHECK(strstr(lv_label_get_text(ui.footer), "仅内存"));
    measurement_page_navigate(&page, 0);
    CHECK(measurement_page_start(&page));
    advance(display, 900); CHECK(page.session.state == MEASUREMENT_FITTING);
    advance(display, 1000); CHECK(page.session.state == MEASUREMENT_COMPLETE);
    CHECK(model.record_count == 1 && model.records[0].result.simulated);
    CHECK(strstr(lv_label_get_text(ui.footer), "仅在内存"));
    CHECK(instrument_ui_set_mode(&ui, MEASUREMENT_RECTANGLE));
    CHECK(measurement_page_start(&page)); advance(display, 1900);
    CHECK(model.record_count == 2 && page.session.result.primary_cmm == 3200);

    ui.services.save = fail_save; ui.services.persistent = true;
    CHECK(!instrument_ui_set_mode(&ui, MEASUREMENT_RING));
    CHECK(model.settings.mode == MEASUREMENT_RECTANGLE && ui.storage_failed);
    ui.services.save = NULL; ui.services.persistent = false;
    for(unsigned cycle = 0; cycle < 20; ++cycle)
        for(unsigned i = 0; i < 4; ++i) { measurement_page_navigate(&page, i); advance(display, 50); }
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    CHECK(flushes > 20 && memory.free_size > 2048);
    printf("PASS: four pages, calibration, ring/rectangle, volatile records, save rollback, repeated redraw\n");
    printf("Host LVGL pool: peak=%zu free=%zu fragmentation=%u%%; flushes=%lu\n",
        memory.max_used, memory.free_size, (unsigned)memory.frag_pct, (unsigned long)flushes);
    lv_deinit(); return 0;
}
