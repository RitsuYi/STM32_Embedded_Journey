#include "measurement_page.h"
#include "ui_draw.h"
/* 固定的LVGL 9.6把命中结果结构放在此头文件；只读取类型定义，不修改第三方库。 */
#include "src/core/lv_obj_event_private.h"
#include "../fonts/measurement_fonts.h"
#include <stdio.h>
#include <string.h>

static const char * const navigation_text[] = {"测量", "标定", "记录", "设置"};
static const char * const error_text[] = {
    "", "设备未连接", "请先完成标定", "测量服务不可用", "测量超时 · 请重试", "数据异常 · 请重试"
};

static const lv_font_t * value_font(const char * text, const lv_font_t * preferred, int32_t width)
{
    lv_point_t size;
    lv_text_get_size(&size, text, preferred, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if(size.x <= width) return preferred;
    lv_text_get_size(&size, text, &measurement_font_24_bold, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x <= width ? &measurement_font_24_bold : &measurement_font_14;
}

static const char * state_text(const MeasurementSession * s)
{
    switch(s->state) {
        case MEASUREMENT_ACQUIRING: return "采集中";
        case MEASUREMENT_FITTING: return "拟合中";
        case MEASUREMENT_COMPLETE: return s->result.status == MEASUREMENT_VALID ? "有效" : "无效";
        case MEASUREMENT_FAILED: return "失败";
        default: return s->connected && s->calibrated ? "待测" : "未就绪";
    }
}

static void draw_header(lv_event_t * e)
{
    MeasurementPage * p = lv_event_get_user_data(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(p->header, &a);
    int32_t x = a.x1, y = a.y1;
    ui_rect(layer, x, y, 480, 36, UI_PANEL, LV_OPA_COVER, 9, 0, 0);
    ui_rect(layer, x, y + 9, 480, 27, UI_PANEL, LV_OPA_COVER, 0, 0, 0);
    ui_text(layer, x + 14, y + 8, 210, &measurement_font_16_bold, UI_WHITE, "智能视觉测量仪", LV_TEXT_ALIGN_LEFT);
    if(p->session.simulated) ui_text(layer, x + 249, y + 10, 101, &measurement_font_12, UI_YELLOW, "模拟测量", LV_TEXT_ALIGN_RIGHT);
    ui_rect(layer, x + 360, y + 15, 8, 8, p->session.connected ? 0x78D4B4 : 0x8895A4,
            LV_OPA_COVER, LV_RADIUS_CIRCLE, 0, 0);
    ui_text(layer, x + 375, y + 10, 43, &measurement_font_12, 0xD6E1EF,
            p->session.connected ? "已连接" : "未连接", LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 427, y + 10, 47, &measurement_font_12, 0xD6E1EF,
            p->session.calibrated ? "已标定" : "未标定", LV_TEXT_ALIGN_LEFT);
}

static void draw_result_panel(lv_event_t * e)
{
    MeasurementPage * p = lv_event_get_user_data(e);
    const MeasurementSession * s = &p->session;
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(p->result_panel, &a);
    int32_t x = a.x1, y = a.y1;
    ui_text(layer, x + 11, y + 6, 132, &measurement_font_14, 0xA4B6CD, "测量结果", LV_TEXT_ALIGN_LEFT);
    ui_line(layer, x + 11, y + 28, x + 143, y + 28, 0x506073, 1);
    bool ring = s->geometry == MEASUREMENT_RING;
    uint32_t value_color = s->state == MEASUREMENT_COMPLETE ? UI_WHITE : UI_UNIT;
    ui_text(layer, x + 12, y + 33, 120, &measurement_font_12, UI_MUTED,
            ring ? "外径" : "长度", LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 12, y + 47, 80, value_font(p->primary_text, &measurement_font_28_bold, 80),
            value_color, p->primary_text, LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 99, y + 59, 40, &measurement_font_14, UI_UNIT, p->inches ? "in" : "mm", LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 12, y + 83, 120, &measurement_font_12, UI_MUTED,
            ring ? "内径" : "宽度", LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 12, y + 96, 73, value_font(p->secondary_text, &measurement_font_24_bold, 73),
            value_color, p->secondary_text, LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 92, y + 105, 40, &measurement_font_14, UI_UNIT, p->inches ? "in" : "mm", LV_TEXT_ALIGN_LEFT);
    ui_line(layer, x + 11, y + 126, x + 143, y + 126, 0x506073, 1);
    ui_text(layer, x + 12, y + 132, 75, &measurement_font_12, UI_MUTED, "拟合残差", LV_TEXT_ALIGN_LEFT);
    ui_text(layer, x + 82, y + 130, 61, &measurement_font_14, value_color, p->residual_text, LV_TEXT_ALIGN_CENTER);
    ui_text(layer, x + 12, y + 152, 75, &measurement_font_12, UI_MUTED, "测量状态", LV_TEXT_ALIGN_LEFT);
    uint32_t badge = measurement_session_busy(s) ? UI_SELECTED :
                     (s->state == MEASUREMENT_FAILED ||
                      (s->state == MEASUREMENT_COMPLETE && s->result.status == MEASUREMENT_INVALID)) ? 0x804B4B :
                     s->state == MEASUREMENT_COMPLETE ? UI_GREEN : 0x506073;
    ui_rect(layer, x + 82, y + 150, 60, 20, badge, LV_OPA_COVER, LV_RADIUS_CIRCLE, 0, 0);
    ui_text(layer, x + 82, y + 152, 60, &measurement_font_12, 0xE4F7F0, state_text(s), LV_TEXT_ALIGN_CENTER);
    ui_text(layer, x + 11, y + 173, 132, &measurement_font_12,
            s->state == MEASUREMENT_FAILED || p->notice_warning ? UI_YELLOW : UI_UNIT, p->hint_text, LV_TEXT_ALIGN_LEFT);
}

static void draw_navigation(lv_event_t * e)
{
    MeasurementPage * p = lv_event_get_user_data(e);
    lv_obj_t * target = lv_event_get_target_obj(e);
    unsigned i;
    for(i = 0; i < 4 && p->navigation[i] != target; ++i) {}
    if(i == 4) return;
    lv_area_t a;
    lv_obj_get_coords(target, &a);
    ui_text(lv_event_get_layer(e), a.x1, a.y1 + (lv_area_get_height(&a) - measurement_font_14.line_height) / 2, lv_area_get_width(&a),
            &measurement_font_14, 0xE4ECF7, navigation_text[i], LV_TEXT_ALIGN_CENTER);
}

static void draw_caption(lv_event_t * e)
{
    MeasurementPage * p = lv_event_get_user_data(e);
    lv_area_t a;
    lv_obj_get_coords(p->caption, &a);
    ui_text(lv_event_get_layer(e), a.x1, a.y1, lv_area_get_width(&a),
            &measurement_font_12, UI_UNIT, p->caption_text, LV_TEXT_ALIGN_CENTER);
}

/* Invalidate only dynamic text, keeping the progress bar and button separate. */
static void invalidate_panel(MeasurementPage * p, int32_t top, int32_t bottom)
{
    lv_area_t a;
    lv_obj_get_coords(p->result_panel, &a);
    a.x1 += 10; a.x2 -= 9; a.y2 = a.y1 + bottom; a.y1 += top;
    lv_obj_invalidate_area(p->result_panel, &a);
}

static void sync_ui(MeasurementPage * p)
{
    const MeasurementSession * s = &p->session;
    const MeasurementSession * old = &p->displayed;
    bool state_changed = s->state != old->state || s->error != old->error;
    if(state_changed) p->notice_warning = false;
    bool device_changed = s->connected != old->connected || s->calibrated != old->calibrated;
    bool result_changed = s->result.primary_cmm != old->result.primary_cmm ||
                          s->result.secondary_cmm != old->result.secondary_cmm ||
                          s->result.residual_cmm != old->result.residual_cmm ||
                          s->result.status != old->result.status;
    bool values_changed = (s->state == MEASUREMENT_COMPLETE) != (old->state == MEASUREMENT_COMPLETE) ||
                          (s->state == MEASUREMENT_COMPLETE && result_changed) || s->geometry != old->geometry ||
                          old->state == (MeasurementState)-1;
    if(values_changed) {
        if(s->state == MEASUREMENT_COMPLETE) {
            if(p->inches) {
                snprintf(p->primary_text, sizeof(p->primary_text), "%.3f", s->result.primary_cmm / 2540.0);
                snprintf(p->secondary_text, sizeof(p->secondary_text), "%.3f", s->result.secondary_cmm / 2540.0);
            }
            else {
                measurement_format_cmm(p->primary_text, sizeof(p->primary_text), (uint32_t)s->result.primary_cmm);
                measurement_format_cmm(p->secondary_text, sizeof(p->secondary_text), (uint32_t)s->result.secondary_cmm);
            }
            measurement_format_cmm(p->residual_text, sizeof(p->residual_text), s->result.residual_cmm);
        }
        else {
            strcpy(p->primary_text, "--"); strcpy(p->secondary_text, "--"); strcpy(p->residual_text, "--");
        }
        invalidate_panel(p, 35, 169);
    }

    if(state_changed || device_changed || result_changed || s->progress != old->progress) {
        if(p->calibration_capture_active)
            snprintf(p->hint_text, sizeof(p->hint_text), "标定采集进行中");
        else if(s->state == MEASUREMENT_FAILED)
            snprintf(p->hint_text, sizeof(p->hint_text), "%s", error_text[s->error]);
        else if(measurement_session_busy(s))
            snprintf(p->hint_text, sizeof(p->hint_text), "%s %u%%",
                     s->state == MEASUREMENT_ACQUIRING ? "采集工件数据" : "计算尺寸", (unsigned)s->progress);
        else if(s->state == MEASUREMENT_COMPLETE)
            snprintf(p->hint_text, sizeof(p->hint_text), "%s",
                     s->result.status == MEASUREMENT_VALID ? "测量完成" : "结果无效 · 请重试");
        else
            snprintf(p->hint_text, sizeof(p->hint_text), "%s",
                     !s->connected ? "设备未连接" : !s->calibrated ? "请先完成标定" : "放置工件后开始测量");
        invalidate_panel(p, 150, 188);
        /* Only the final step to 100 has a short, finite LVGL animation.
         * Periodic progress uses no animation allocation/restart. */
        if(lv_bar_get_value(p->progress_bar) != s->progress)
            lv_bar_set_value(p->progress_bar, s->progress,
                             s->state == MEASUREMENT_COMPLETE ? LV_ANIM_ON : LV_ANIM_OFF);
    }

    if(state_changed || result_changed || s->geometry != old->geometry || s->has_result != old->has_result) {
        const char * geometry = s->geometry == MEASUREMENT_RING ? "圆环" : "矩形";
        const char * caption = s->state == MEASUREMENT_COMPLETE ?
                              (s->result.status == MEASUREMENT_VALID ? "本次轮廓" : "无效轮廓") :
                              s->has_result ? "上次轮廓" : "等待测量";
        snprintf(p->caption_text, sizeof(p->caption_text), "%s · %s", caption, geometry);
        lv_obj_invalidate(p->caption);
        const char * button = p->calibration_capture_active ? "标定采集中" : measurement_session_busy(s) ? "测量中..." :
                              s->state == MEASUREMENT_COMPLETE ? "再次测量" :
                              s->state == MEASUREMENT_FAILED ? "重新测量" : "开始测量";
        if(strcmp(lv_label_get_text(p->start_label), button) != 0) lv_label_set_text_static(p->start_label, button);
    }
    lv_obj_set_state(p->start_button, LV_STATE_DISABLED,
                     measurement_session_busy(s) || !s->connected || !s->calibrated ||
                     !p->backend.start || p->selected_navigation != 0 || p->calibration_capture_active);
    if(device_changed) lv_obj_invalidate(p->header);
    p->displayed = *s;
}

static void navigation_click(lv_event_t * e)
{
    MeasurementPage * p = lv_event_get_user_data(e);
    lv_obj_t * target = lv_event_get_target_obj(e);
    for(unsigned i = 0; i < 4; ++i) {
        if(p->navigation[i] != target || p->selected_navigation == i) continue;
        measurement_page_navigate(p, i);
        break;
    }
}

/* 命中检测只决定“这个坐标是否算点中”，不修改芯片坐标，也不主动产生点击。
 * 四键保留原来左右/上方各4像素的容错，下方则覆盖到480×320屏幕的最后一行。
 * 横向不跟着扩11像素，避免相邻导航按钮的判定范围互相重叠。 */
static void navigation_hit_test(lv_event_t * e)
{
    lv_hit_test_info_t * hit = lv_event_get_hit_test_info(e);
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(e), &area);
    area.x1 -= 4; area.x2 += 4;
    area.y1 -= 4; area.y2 = 319;
    hit->res = hit->point->x >= area.x1 && hit->point->x <= area.x2 &&
               hit->point->y >= area.y1 && hit->point->y <= area.y2;
}

static void start_click(lv_event_t * e)
{
    measurement_page_start(lv_event_get_user_data(e));
}

static lv_obj_t * plain_object(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y); lv_obj_set_size(object, w, h);
    lv_obj_set_scrollable(object, false);
    lv_obj_set_clickable(object, false);
    return object;
}

static void button_styles(lv_obj_t * button)
{
    lv_obj_set_style_bg_color(button, ui_color(UI_SELECTED), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, ui_color(0x598DAB), LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(button, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, ui_color(0x79B6CD), 0);
    lv_obj_set_style_border_color(button, ui_color(UI_CYAN), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, ui_color(0x344558), LV_STATE_DISABLED);
    lv_obj_set_style_border_color(button, ui_color(UI_BORDER), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, ui_color(UI_WHITE), 0);
    lv_obj_set_style_text_color(button, ui_color(0x8396A8), LV_STATE_DISABLED);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
}

void measurement_page_create(MeasurementPage * p, lv_obj_t * screen, const MeasurementResult * initial)
{
    memset(p, 0, sizeof(*p));
    MeasurementGeometry geometry = initial && initial->geometry == MEASUREMENT_RECTANGLE ?
                                   MEASUREMENT_RECTANGLE : MEASUREMENT_RING;
    measurement_session_init(&p->session, geometry, initial && initial->connected,
                             initial && initial->calibrated, initial && initial->simulated);
    p->displayed.state = (MeasurementState)-1;
    p->root = plain_object(screen, 0, 0, 480, 320);
    lv_obj_set_style_bg_color(p->root, ui_color(UI_BG), 0);
    lv_obj_set_style_bg_opa(p->root, LV_OPA_COVER, 0);
    p->header = plain_object(p->root, 0, 0, 480, 36);
    lv_obj_add_event_cb(p->header, draw_header, LV_EVENT_DRAW_MAIN, p);
    geometry_view_create(&p->geometry, p->root);
    p->caption = plain_object(p->geometry.object, 61, 210, 231, measurement_font_12.line_height);
    lv_obj_add_event_cb(p->caption, draw_caption, LV_EVENT_DRAW_MAIN, p);

    p->result_panel = plain_object(p->root, 316, 43, 154, 229);
    lv_obj_set_style_bg_color(p->result_panel, ui_color(UI_PANEL), 0);
    lv_obj_set_style_bg_opa(p->result_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p->result_panel, 5, 0);
    lv_obj_add_event_cb(p->result_panel, draw_result_panel, LV_EVENT_DRAW_MAIN, p);
    p->progress_bar = lv_bar_create(p->result_panel);
    lv_obj_remove_style_all(p->progress_bar);
    lv_obj_set_pos(p->progress_bar, 11, 190); lv_obj_set_size(p->progress_bar, 132, 3);
    lv_obj_set_style_bg_opa(p->progress_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p->progress_bar, ui_color(UI_BORDER), 0);
    lv_obj_set_style_bg_opa(p->progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(p->progress_bar, ui_color(UI_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(p->progress_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(p->progress_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(p->progress_bar, 160, 0);
    lv_obj_set_clickable(p->progress_bar, false);
    lv_bar_set_range(p->progress_bar, 0, 100);

    p->start_button = lv_button_create(p->result_panel);
    lv_obj_remove_style_all(p->start_button);
    lv_obj_set_pos(p->start_button, 11, 196); lv_obj_set_size(p->start_button, 132, 29);
    button_styles(p->start_button);
    /* 测量按钮四周最多额外8像素可点；超出结果面板的部分仍由父容器限制。 */
    lv_obj_set_ext_click_area(p->start_button, 8);
    lv_obj_add_event_cb(p->start_button, start_click, LV_EVENT_CLICKED, p);
    p->start_label = lv_label_create(p->start_button);
    lv_obj_set_style_text_font(p->start_label, &measurement_font_14, 0);
    lv_label_set_text_static(p->start_label, "开始测量");
    lv_obj_center(p->start_label);

    lv_obj_t * navigation_panel = plain_object(p->root, 7, 280, 463, 33);
    lv_obj_set_style_bg_color(navigation_panel, ui_color(UI_PANEL), 0);
    lv_obj_set_style_bg_opa(navigation_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(navigation_panel, 5, 0);
    for(unsigned i = 0; i < 4; ++i) {
        /* 按钮直接归整屏容器管理，坐标换成原来的绝对位置，外观不变。
         * 原导航背景只到Y312；继续把按钮放在其内部会使Y313～319无法被搜索到。 */
        p->navigation[i] = lv_button_create(p->root);
        lv_obj_remove_style_all(p->navigation[i]);
        lv_obj_set_pos(p->navigation[i], 13 + (int32_t)i * 112, 284);
        lv_obj_set_size(p->navigation[i], 101, 25);
        /* LVGL先搜索四周11像素，再由上面的回调收窄左右/上方，只向下扩到屏幕底边。 */
        lv_obj_set_ext_click_area(p->navigation[i], 11);
        lv_obj_set_adv_hittest(p->navigation[i], true);
        lv_obj_add_event_cb(p->navigation[i], navigation_hit_test, LV_EVENT_HIT_TEST, NULL);
        lv_obj_set_style_radius(p->navigation[i], 4, 0);
        lv_obj_set_style_bg_opa(p->navigation[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(p->navigation[i], LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(p->navigation[i], ui_color(UI_SELECTED), LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(p->navigation[i], LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(p->navigation[i], ui_color(0x598DAB), LV_STATE_PRESSED);
        lv_obj_add_event_cb(p->navigation[i], draw_navigation, LV_EVENT_DRAW_MAIN, p);
        lv_obj_add_event_cb(p->navigation[i], navigation_click, LV_EVENT_CLICKED, p);
    }
    lv_obj_set_checked(p->navigation[0], true);
    sync_ui(p);
}

void measurement_page_navigate(MeasurementPage * p, unsigned destination)
{
    if(!p || destination > 3 || p->selected_navigation == destination) return;
    lv_obj_set_checked(p->navigation[p->selected_navigation], false);
    p->selected_navigation = destination;
    lv_obj_set_checked(p->navigation[destination], true);
    lv_obj_set_hidden(p->geometry.object, destination != 0);
    lv_obj_set_hidden(p->result_panel, destination != 0);
    if(p->navigation_changed) p->navigation_changed(p->application_context, destination);
    sync_ui(p);
}

void measurement_page_set_units(MeasurementPage * p, bool inches)
{
    if(!p || p->inches == inches) return;
    p->inches = inches;
    p->displayed.state = (MeasurementState)-1;
    geometry_view_set_options(&p->geometry, p->geometry.show_grid, p->geometry.show_dimensions, p->geometry.show_contour, inches);
    sync_ui(p);
    lv_obj_invalidate(p->result_panel);
}

void measurement_page_set_calibration_capture(MeasurementPage * p, bool active)
{
    if(!p || p->calibration_capture_active == active) return;
    p->calibration_capture_active = active;
    p->displayed.state = (MeasurementState)-1; sync_ui(p);
}
void measurement_page_invalidate_result(MeasurementPage * p)
{
    if(!p || measurement_session_busy(&p->session)) return;
    p->session.state = MEASUREMENT_IDLE; p->session.error = MEASUREMENT_ERROR_NONE;
    p->session.has_result = false; p->session.progress = 0;
    p->session.result = (MeasurementResult){.geometry = p->session.geometry, .status = MEASUREMENT_PENDING};
    geometry_view_clear(&p->geometry); sync_ui(p);
}
void measurement_page_show_notice(MeasurementPage * p, const char * text, bool warning)
{
    if(!p || !text) return;
    snprintf(p->hint_text, sizeof(p->hint_text), "%s", text); p->notice_warning = warning;
    invalidate_panel(p, 150, 188);
}

void measurement_page_set_backend(MeasurementPage * p, const MeasurementBackend * backend)
{
    if(!p || measurement_session_busy(&p->session)) return;
    p->backend = backend ? *backend : (MeasurementBackend){0};
    sync_ui(p);
}

bool measurement_page_start(MeasurementPage * p)
{
    if(!p || !p->root || p->selected_navigation != 0 || measurement_session_busy(&p->session) || p->calibration_capture_active) return false;
    if(!measurement_session_start(&p->session, lv_tick_get())) { sync_ui(p); return false; }
    sync_ui(p); /* feedback is immediate, even before the backend returns */
    uint32_t id = p->session.request_id;
    if(!p->backend.start || !p->backend.start(p->backend.context, id, p->session.geometry)) {
        measurement_page_fail(p, id, MEASUREMENT_ERROR_BACKEND);
        return false;
    }
    return true;
}

bool measurement_page_select_geometry(MeasurementPage * p, MeasurementGeometry geometry)
{
    if(!p || !p->root || !measurement_session_select(&p->session, geometry)) return false;
    if(p->geometry.has_geometry && p->geometry.result.geometry != geometry) geometry_view_clear(&p->geometry);
    sync_ui(p);
    return true;
}

bool measurement_page_progress(MeasurementPage * p, uint32_t id, MeasurementState phase, uint8_t progress)
{
    measurement_page_process(p, lv_tick_get());
    if(!p || !measurement_session_progress(&p->session, id, phase, progress)) return false;
    sync_ui(p);
    return true;
}

bool measurement_page_complete(MeasurementPage * p, uint32_t id, const MeasurementResult * result)
{
    measurement_page_process(p, lv_tick_get());
    if(!p || !measurement_session_busy(&p->session) || p->session.request_id != id) return false;
    GeometryLayout layout;
    if(!geometry_layout_calculate(result, p->geometry.scale_q8, &layout)) {
        measurement_page_fail(p, id, MEASUREMENT_ERROR_RESULT);
        return false;
    }
    if(!measurement_session_complete(&p->session, id, result)) {
        if(p->backend.cancel) p->backend.cancel(p->backend.context, id);
        sync_ui(p);
        return false;
    }
    geometry_view_update(&p->geometry, &p->session.result);
    sync_ui(p);
    if(p->result_completed) p->result_completed(p->application_context, &p->session.result);
    return true;
}

bool measurement_page_fail(MeasurementPage * p, uint32_t id, MeasurementError error)
{
    if(!p || !measurement_session_fail(&p->session, id, error)) return false;
    if(p->backend.cancel) p->backend.cancel(p->backend.context, id);
    sync_ui(p);
    return true;
}

void measurement_page_set_device(MeasurementPage * p, bool connected, bool calibrated)
{
    if(!p) return;
    bool was_busy = measurement_session_busy(&p->session);
    measurement_session_set_device(&p->session, connected, calibrated);
    if(was_busy && !measurement_session_busy(&p->session) && p->backend.cancel)
        p->backend.cancel(p->backend.context, p->session.request_id);
    sync_ui(p);
}

void measurement_page_process(MeasurementPage * p, uint32_t now_ms)
{
    if(!p || !measurement_session_process(&p->session, now_ms)) return;
    if(p->backend.cancel) p->backend.cancel(p->backend.context, p->session.request_id);
    sync_ui(p);
}

bool measurement_page_set_scale(MeasurementPage * p, uint32_t scale)
{
    return p && geometry_view_set_scale(&p->geometry, scale);
}
