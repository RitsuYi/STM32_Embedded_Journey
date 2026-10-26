#include "lvgl_port.h"
#include "gui_demo.h"
#include "ST7796.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h" /* 只读实际单键布局，验证扩展不改变外观。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

/* 此测试把真实lvgl_port和四页面应用交给LVGL 9.6运行，只替代显示和触摸硬件。
 * 不直接发送CLICKED，只有按下/松手报文能够产生按钮事件，覆盖以前漏测的命中流程。 */
static uint32_t now_ms;
static int32_t touch_x, touch_y;
static bool pressed, read_valid = true;
static uint32_t reader_calls;
/* 事件计数仅供主机测试观察，正式固件不再保存调试统计。 */
static uint32_t pressed_events, released_events, clicked_events;
static void observe_input(lv_event_t *event)
{
    switch(lv_event_get_code(event)) {
        case LV_EVENT_PRESSED: ++pressed_events; break;
        case LV_EVENT_RELEASED: ++released_events; break;
        case LV_EVENT_CLICKED: ++clicked_events; break;
        default: break;
    }
}
static MeasurementPage page;
static InstrumentModel model;
static InstrumentUI ui;
/* 观察矩阵真正发出的键值事件，区分“点中了键盘”与“确实输入了目标键”。 */
static uint32_t keyboard_values, keyboard_last_key;
static void observe_keyboard(lv_event_t *event)
{
    ++keyboard_values;
    keyboard_last_key = lv_keyboard_get_selected_button(lv_event_get_target_obj(event));
}

uint32_t HAL_GetTick(void) { return now_ms; }
void GUI_LVGL_AssertFailed(const char *file, int line)
{
    fprintf(stderr, "LVGL assert: %s:%d\n", file, line);
    exit(1);
}
/* 触摸回归只替代硬件启动/等待，不跳过真实LVGL刷新和输入定时器。 */
HAL_StatusTypeDef ST7796_DMAInit(void) { return HAL_OK; }
HAL_StatusTypeDef ST7796_WriteRGB565Wait(void) { return HAL_OK; }
HAL_StatusTypeDef ST7796_WriteRGB565Start(uint16_t x, uint16_t y, uint16_t width,
    uint16_t height, const uint8_t *pixels, uint32_t stride)
{
    CHECK(x + width <= 480 && y + height <= 320 && pixels && stride >= width * 2U);
    return HAL_OK;
}
static bool reader(void *context, int32_t *x, int32_t *y, bool *down)
{
    (void)context;
    ++reader_calls;
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
/* 通过真实指针在指定坐标按下/松手，可验证按钮外面的新增命中区确实能触发原操作。 */
static void tap_at(lv_obj_t *button, int32_t x, int32_t y)
{
    lv_obj_update_layout(lv_screen_active());
    touch_x = x; touch_y = y;
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

/* 常规点击仍取可见按钮中心，便于保留原有导航/测量回归。 */
static void tap(lv_obj_t *button)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area;
    lv_obj_get_coords(button, &area);
    tap_at(button, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}

/* 每步恰好让真实输入定时器到期一次，检查实际LVGL输入状态。
 * 不直接调用输入回调，也不发送CLICKED事件。 */
static void report(lv_indev_t *input, int32_t x, int32_t y, bool down)
{
    touch_x = x; touch_y = y; pressed = down;
    lv_timer_reset(lv_indev_get_read_timer(input));
    advance(15);
}

/* 简化版输入不再猜位置：驱动给出的有效CONTACT点必须第一帧原样交给LVGL。 */
static void check_direct_input(lv_indev_t *input)
{
    const int32_t y_values[] = {303, 316, 268, 0, 319, 160};
    for (unsigned i = 0; i < sizeof(y_values) / sizeof(y_values[0]); ++i) {
        report(input, 177, y_values[i], true);
        lv_point_t position;
        lv_indev_get_point(input, &position);
        CHECK(lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED && position.x == 177 && position.y == y_values[i]);
    }
    report(input, 177, 160, false);
    CHECK((lv_indev_get_state(input) == LV_INDEV_STATE_RELEASED));
    /* 读失败时不造按下，也不补交之前不存在的点击。 */
    uint32_t clicks = clicked_events;
    read_valid = false; report(input, 177, 303, true);
    CHECK((lv_indev_get_state(input) == LV_INDEV_STATE_RELEASED) && clicked_events == clicks);
    read_valid = true; report(input, 177, 303, false);
    /* 一帧有效CONTACT就够产生按下；真实松手立刻释放，不额外读取硬件。 */
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area; lv_obj_get_coords(page.navigation[2], &area);
    int32_t x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
    uint32_t calls = reader_calls;
    clicks = clicked_events;
    report(input, x, y, true);
    CHECK((lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED) && lv_obj_has_state(page.navigation[2], LV_STATE_PRESSED));
    report(input, x, y, false);
    CHECK(page.selected_navigation == 2 && clicked_events == clicks + 1U);
    CHECK(reader_calls == calls + 2U && (lv_indev_get_state(input) == LV_INDEV_STATE_RELEASED));
    CHECK(lv_indev_get_cursor(input) == NULL);
}

/* 检查新增容错区、相邻行边界、禁用状态及弹窗遮挡，防止扩大后误触别的功能。
 * 硬件只用坐标替身；所有操作仍经过实际LVGL命中和输入定时器。 */
static void check_expanded_hit_areas(lv_indev_t *input)
{
    lv_area_t area, next;
    for(unsigned i = 0; i < 4U; ++i) {
        lv_obj_get_coords(page.navigation[i], &area);
        CHECK(area.x1 == 13 + (int32_t)i * 112 && area.y1 == 284);
        CHECK(lv_area_get_width(&area) == 101 && lv_area_get_height(&area) == 25);
        /* 下方空白的每个像素都应归本键，包括屏幕最后一行、左右原有4像素容错区。 */
        for(int32_t y = area.y2 + 1; y <= 319; ++y) {
            for(int32_t x = area.x1 - 4; x <= area.x2 + 4; ++x) {
                lv_point_t point = {x, y};
                CHECK(lv_indev_search_obj(lv_screen_active(), &point) == page.navigation[i]);
            }
        }
        lv_point_t outside = {area.x2 + 5, 319};
        CHECK(!lv_obj_hit_test(page.navigation[i], &outside));
        tap_at(page.navigation[i], (area.x1 + area.x2) / 2, 319);
        CHECK(page.selected_navigation == i);
    }
    /* 从可见按钮按住后偏移到屏幕底边，仍在同一个命中区，应正常松手切页。 */
    lv_obj_get_coords(page.navigation[1], &area);
    uint32_t drift_clicks = clicked_events;
    report(input, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2, true);
    report(input, (area.x1 + area.x2) / 2, 319, true);
    report(input, (area.x1 + area.x2) / 2, 319, false);
    CHECK(page.selected_navigation == 1 && clicked_events == drift_clicks + 1U);
    tap(page.navigation[0]);
    lv_obj_get_coords(page.start_button, &area);
    lv_point_t start = {area.x1 - 6, (area.y1 + area.y2) / 2};
    CHECK(lv_indev_search_obj(lv_screen_active(), &start) == page.start_button);
    /* 将“开始测量”置为禁用，确认外扩区域仍遵守按钮的禁用限制。 */
    lv_obj_add_state(page.start_button, LV_STATE_DISABLED);
    uint32_t clicks = clicked_events;
    report(input, start.x, start.y, true); report(input, start.x, start.y, false);
    CHECK(!measurement_session_busy(&page.session) && clicked_events == clicks);
    lv_obj_remove_state(page.start_button, LV_STATE_DISABLED);

    tap(page.navigation[2]);
    /* 记录行间隙只有2像素：两行各接收靠近自己的1像素，不能互相抢占。 */
    lv_obj_get_coords(ui.rows[0], &area); lv_obj_get_coords(ui.rows[1], &next);
    lv_point_t gap = {area.x1 + 20, area.y2 + 1};
    CHECK(lv_indev_search_obj(lv_screen_active(), &gap) == ui.rows[0]);
    CHECK(!lv_obj_hit_test(ui.rows[1], &gap));
    ++gap.y;
    CHECK(lv_indev_search_obj(lv_screen_active(), &gap) == ui.rows[1]);
    CHECK(!lv_obj_hit_test(ui.rows[0], &gap) && gap.y == next.y1 - 1);
    tap_at(ui.rows[0], area.x1 - 6, (area.y1 + area.y2) / 2);
    CHECK(ui.selected_record == model.records[0].id);
    tap(ui.actions[0]);
    lv_obj_get_coords(ui.actions[1], &area);
    tap_at(ui.actions[1], area.x1 - 6, (area.y1 + area.y2) / 2);
    CHECK(ui.confirm == CONFIRM_CLEAR);
    /* 确认层必须挡住底部导航扩展区；取消按钮也应支持外扩点击。 */
    lv_obj_get_coords(page.navigation[0], &area);
    lv_point_t bottom = {(area.x1 + area.x2) / 2, 319};
    CHECK(lv_indev_search_obj(lv_screen_active(), &bottom) == ui.confirmation);
    lv_obj_t *cancel = lv_obj_get_child(ui.confirmation, 2);
    lv_obj_get_coords(cancel, &area);
    tap_at(cancel, area.x1 - 6, (area.y1 + area.y2) / 2);
    CHECK(ui.confirm == CONFIRM_NONE && model.record_count == 3U);

    tap(page.navigation[3]);
    lv_obj_get_coords(ui.rows[0], &area);
    tap_at(ui.rows[0], area.x1 - 6, (area.y1 + area.y2) / 2);
    CHECK(ui.settings_page == SETTINGS_MEASUREMENT);
    lv_obj_get_coords(ui.back, &area);
    tap_at(ui.back, area.x1 - 6, (area.y1 + area.y2) / 2);
    CHECK(ui.settings_page == SETTINGS_MENU);
    tap(ui.rows[0]);
    lv_obj_get_coords(ui.rows[1], &area);
    tap_at(ui.rows[1], area.x1 - 6, (area.y1 + area.y2) / 2);
    CHECK(ui.edit == EDIT_SAMPLES);
    CHECK(lv_indev_search_obj(lv_screen_active(), &bottom) != page.navigation[0]);
    CHECK(lv_mem_test() == LV_RESULT_OK);
}

/* 只提交真实指针按下/松手，要求原键盘发出一次且仅一次对应键值事件。 */
static void keyboard_tap(lv_indev_t *input, int32_t x, int32_t y, uint32_t key)
{
    lv_point_t point = {x, y};
    CHECK(lv_indev_search_obj(lv_screen_active(), &point) == ui.keyboard);
    uint32_t before = keyboard_values;
    report(input, x, y, true);
    CHECK(lv_keyboard_get_selected_button(ui.keyboard) == key);
    report(input, x, y, false);
    CHECK(keyboard_values == before + 1U && keyboard_last_key == key);
}

/* 五键各自下方每一行都要与原键位产生相同输入；范围、边界、长按和拖动都走真实LVGL。
 * 正负号沿用原输入框的字符限制，只扩命中范围，不修改字段允许输入的正数规则。 */
static void check_keyboard_bottom(lv_indev_t *input)
{
    CHECK(ui.edit == EDIT_SAMPLES && !lv_obj_is_hidden(ui.editor));
    lv_obj_update_layout(lv_screen_active());
    const lv_buttonmatrix_t *matrix = (const lv_buttonmatrix_t *)ui.keyboard;
    CHECK(matrix->btn_cnt == 17U);
    lv_area_t before_areas[17];
    memcpy(before_areas, matrix->button_areas, sizeof(before_areas));
    lv_area_t body;
    lv_obj_get_coords(ui.keyboard, &body);
    CHECK(body.x1 == 7 && body.y1 == 121 && body.x2 == 472 && body.y2 == 312);
    const char * const names[5] = {"+/-", "0", ".", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT};
    int32_t centers[5];
    for(unsigned i = 0; i < 5U; ++i) {
        uint32_t key = 12U + i;
        CHECK(strcmp(lv_keyboard_get_button_text(ui.keyboard, key), names[i]) == 0);
        const lv_area_t *area = &before_areas[key];
        int32_t x = body.x1 + (area->x1 + area->x2) / 2;
        int32_t y = body.y1 + (area->y1 + area->y2) / 2;
        centers[i] = x;
        const char *initial = i < 3U ? "12" : "123";
        uint32_t cursor = i == 3U ? 2U : i == 4U ? 1U : 2U;
        lv_textarea_set_text(ui.textarea, initial); lv_textarea_set_cursor_pos(ui.textarea, cursor);
        keyboard_tap(input, x, y, key);
        char expected[16];
        snprintf(expected, sizeof(expected), "%s", lv_textarea_get_text(ui.textarea));
        uint32_t expected_cursor = lv_textarea_get_cursor_pos(ui.textarea);
        if(i == 1U) CHECK(strcmp(expected, "120") == 0);
        if(i == 2U) CHECK(strcmp(expected, "12.") == 0);
        if(i == 3U) CHECK(expected_cursor == 1U);
        if(i == 4U) CHECK(expected_cursor == 2U);
        /* 连同原键盘内部的底部留白一起检查，确保从可见键底边到屏幕底边没有断层。 */
        for(y = body.y1 + area->y2 + 1; y <= 319; ++y) {
            lv_textarea_set_text(ui.textarea, initial); lv_textarea_set_cursor_pos(ui.textarea, cursor);
            keyboard_tap(input, x, y, key);
            CHECK(strcmp(lv_textarea_get_text(ui.textarea), expected) == 0);
            CHECK(lv_textarea_get_cursor_pos(ui.textarea) == expected_cursor);
        }
        /* 相邻两键空隙中线两侧必须分别命中各自键，不能两个键都接收。 */
        if(i < 4U) {
            int32_t split = body.x1 + (area->x2 + before_areas[key + 1U].x1) / 2;
            lv_textarea_set_text(ui.textarea, "12"); keyboard_tap(input, split, 319, key);
            lv_textarea_set_text(ui.textarea, "12"); keyboard_tap(input, split + 1, 319, key + 1U);
        }
    }
    /* 上方三排仍使用各自原键位；外框扩大不能顺便扩大顶部或左右。 */
    for(unsigned row = 0; row < 3U; ++row) {
        uint32_t key = row * 4U;
        const lv_area_t *area = &before_areas[key];
        lv_textarea_set_text(ui.textarea, "");
        keyboard_tap(input, body.x1 + (area->x1 + area->x2) / 2,
                     body.y1 + (area->y1 + area->y2) / 2, key);
        CHECK(lv_textarea_get_text(ui.textarea)[0] == (char)('1' + row * 3U));
    }
    lv_point_t outside = {body.x1 - 1, 319}; CHECK(!lv_obj_hit_test(ui.keyboard, &outside));
    outside = (lv_point_t){centers[0], body.y1 - 1}; CHECK(!lv_obj_hit_test(ui.keyboard, &outside));
    outside = (lv_point_t){centers[0], 320}; CHECK(!lv_obj_hit_test(ui.keyboard, &outside));
    /* 禁用底行“0”时，新增空白也不得输入；恢复后长按仍沿用原矩阵重复功能。 */
    lv_buttonmatrix_set_button_ctrl(ui.keyboard, 13, LV_BUTTONMATRIX_CTRL_DISABLED);
    uint32_t values = keyboard_values;
    report(input, centers[1], 319, true); report(input, centers[1], 319, false);
    CHECK(keyboard_values == values);
    lv_buttonmatrix_clear_button_ctrl(ui.keyboard, 13, LV_BUTTONMATRIX_CTRL_DISABLED);
    lv_textarea_set_text(ui.textarea, "1");
    report(input, centers[1], 319, true); advance(700);
    report(input, centers[1], 319, false);
    CHECK(keyboard_values > values + 1U && strlen(lv_textarea_get_text(ui.textarea)) > 2U);
    /* 按住0后偏到底边仍属同键；横滑到小数点列则取消，不能重复输入0或补点号。 */
    lv_textarea_set_text(ui.textarea, "1");
    const lv_area_t *zero = &before_areas[13];
    report(input, centers[1], body.y1 + (zero->y1 + zero->y2) / 2, true);
    report(input, centers[1], 319, true);
    CHECK(lv_keyboard_get_selected_button(ui.keyboard) == 13U);
    values = keyboard_values;
    report(input, centers[2], 319, true); advance(700);
    report(input, centers[2], 319, false);
    CHECK(keyboard_values == values && strcmp(lv_textarea_get_text(ui.textarea), "10") == 0);
    CHECK(memcmp(before_areas, matrix->button_areas, sizeof(before_areas)) == 0);
    /* 原收起键仍能关闭编辑框；关闭后原四导航的底边命中不受新逻辑影响。 */
    const lv_area_t *cancel = &before_areas[3];
    keyboard_tap(input, body.x1 + (cancel->x1 + cancel->x2) / 2,
                 body.y1 + (cancel->y1 + cancel->y2) / 2, 3);
    CHECK(ui.edit == EDIT_NONE && lv_obj_is_hidden(ui.editor));
    CHECK(lv_indev_get_cursor(input) == NULL && lv_mem_test() == LV_RESULT_OK);
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_display_t *display = GUI_DisplayInit();
    CHECK(display);
    lv_indev_t *input = GUI_RegisterTouch(reader, NULL);
    CHECK(input && lv_indev_get_display(input) == display);
    CHECK(lv_indev_add_event_cb(input, observe_input, LV_EVENT_ALL, NULL));
    CHECK(lv_indev_get_cursor(input) == NULL);
    CHECK(lv_obj_get_child_count(lv_display_get_layer_sys(display)) == 0U);
    instrument_defaults(&model);
    const MeasurementResult initial = {.geometry = MEASUREMENT_RING,
        .status = MEASUREMENT_PENDING, .connected = true, .simulated = true};
    measurement_page_create(&page, lv_screen_active(), &initial);
    GUI_DemoInit(&page, &model);
    const InstrumentServices services = {.capture_calibration = GUI_DemoCalibration};
    instrument_ui_create(&ui, &page, &model, &services, "模拟测量 / 仅内存");
    CHECK(lv_obj_add_event_cb(ui.keyboard, observe_keyboard, LV_EVENT_VALUE_CHANGED, NULL));
    advance(100);

    /* 底部导航不依赖标定，必须逐个能由真实触摸状态切换。 */
    for (unsigned i = 1; i < 4; ++i) {
        tap(page.navigation[i]);
        CHECK(page.selected_navigation == i);
    }
    tap(page.navigation[0]);
    CHECK(page.selected_navigation == 0);
    CHECK(reader_calls > 0);
    CHECK(pressed_events == 4 && released_events == 4);
    CHECK(clicked_events == 4);
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
    CHECK(clicked_events == 5);
    /* 20ms短按应被独立的15ms输入周期采到；检查导航栏上下留白的4像素扩展区。
     * 仍只输入坐标和按下/松手，不手工发送点击，防止测试绕过真正命中过程。 */
    for (unsigned i = 0; i < 4U; ++i) {
        lv_obj_get_coords(page.navigation[i], &area);
        touch_x = (area.x1 + area.x2) / 2;
        touch_y = i % 2U ? area.y1 - 3 : area.y2 + 3;
        lv_point_t edge = {touch_x, touch_y};
        CHECK(lv_indev_search_obj(lv_screen_active(), &edge) == page.navigation[i]);
        uint32_t clicks_before = clicked_events;
        pressed = true;
        advance(20);
        CHECK(lv_obj_has_state(page.navigation[i], LV_STATE_PRESSED));
        pressed = false;
        advance(80);
        CHECK(page.selected_navigation == i);
        CHECK(clicked_events == clicks_before + 1U);
    }
    /* 复现用户操作顺序：真实触摸完成标定、启动模拟测量，再等到结果出现。
     * 不能只检查画面完成；还必须证明输入定时器继续取样，并能再次点击导航。
     * 显示替身不会模拟板上的I2C故障，因此本测试只验证应用是否误停输入设备。 */
    tap(page.navigation[1]);
    for (unsigned i = 0; i < 3U; ++i) tap(ui.actions[0]);
    advance(1500);
    CHECK(ui.candidate.valid);
    tap(ui.actions[0]);
    CHECK(instrument_calibration_ready(&model));
    tap(page.navigation[0]);
    for (unsigned cycle = 0; cycle < 3U; ++cycle) {
        /* 第一轮从测量按钮左侧新增6像素处启动，其余轮次保留原中心点击回归。 */
        if(cycle == 0U) {
            lv_obj_get_coords(page.start_button, &area);
            tap_at(page.start_button, area.x1 - 6, (area.y1 + area.y2) / 2);
        }
        else tap(page.start_button);
        CHECK(measurement_session_busy(&page.session));
        advance(2200);
        CHECK(page.session.state == MEASUREMENT_COMPLETE);
        CHECK(model.record_count == cycle + 1U);
        uint32_t reads_before = reader_calls;
        advance(200);
        CHECK(reader_calls > reads_before + 5U);
        CHECK(!lv_timer_get_paused(lv_indev_get_read_timer(input)));
        CHECK(lv_mem_test() == LV_RESULT_OK);
        tap(page.navigation[2]);
        CHECK(page.selected_navigation == 2);
        tap(page.navigation[0]);
        CHECK(page.selected_navigation == 0);
    }
    check_direct_input(input);
    check_expanded_hit_areas(input);
    check_keyboard_bottom(input);
    CHECK(pressed_events == released_events);
    puts("PASS: pointer navigation, calibration, continued input and five-key bottom extension");
    lv_deinit();
    return 0;
}
