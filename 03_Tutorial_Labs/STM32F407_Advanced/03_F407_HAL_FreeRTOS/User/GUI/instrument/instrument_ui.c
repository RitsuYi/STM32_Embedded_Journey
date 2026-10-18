#include "instrument_ui.h"
#include "../measurement/ui_draw.h"
/* 固定的LVGL 9.6把命中结果结构放在此头文件；只读取类型定义，不修改第三方库。 */
#include "src/core/lv_obj_event_private.h"
/* 固定LVGL 9.6的键盘没有公开单键坐标接口，仅读取矩阵布局，不改库内按键区域。 */
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "../fonts/measurement_fonts.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

enum { PANEL_WIDTH = 463, CONTENT_WIDTH = PANEL_WIDTH - 16 };

static void render(InstrumentUI * u);
static lv_obj_t * box(lv_obj_t * parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o); lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, ui_color(color), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(o, false); lv_obj_set_clickable(o, false);
    lv_obj_set_style_radius(o, 5, 0);
    return o;
}
static lv_obj_t * label(lv_obj_t * parent, int x, int y, int width, const lv_font_t * font, uint32_t color)
{
    lv_obj_t * l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y); lv_obj_set_width(l, width);
    lv_obj_set_style_text_font(l, font, 0); lv_obj_set_style_text_color(l, ui_color(color), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP); lv_label_set_text(l, "");
    return l;
}

/* 通用按钮在外观之外最多多接受8像素的点击，容忍手指落点的小偏差。
 * 命中检测时读取当前布局：相邻按钮之间以空隙的中线为界，各用靠近自己的一半。
 * 这样设置列表即使只有2像素行距，也不会因为扩大范围而点到上一行/下一行。
 * e是LVGL提供的本次检测事件；只写回hit->res，不改坐标、不手工发送CLICKED。 */
static void button_hit_test(lv_event_t * e)
{
    lv_obj_t * target = lv_event_get_target_obj(e);
    lv_hit_test_info_t * hit = lv_event_get_hit_test_info(e);
    lv_area_t body, area;
    lv_obj_get_coords(target, &body);
    area = body;
    lv_area_increase(&area, 8, 8);
    lv_obj_t * parent = lv_obj_get_parent(target);
    uint32_t count = lv_obj_get_child_count(parent);
    for(uint32_t i = 0; i < count; ++i) {
        lv_obj_t * other = lv_obj_get_child(parent, (int32_t)i);
        /* 隐藏项和只显示数据的行不参与划分；禁用按钮仍保留自己的位置，防止被旁键抢占。 */
        if(other == target || lv_obj_is_hidden(other) || !lv_obj_is_clickable(other) ||
           !lv_obj_check_type(other, &lv_button_class)) continue;
        lv_area_t peer;
        lv_obj_get_coords(other, &peer);
        if(body.x1 <= peer.x2 && peer.x1 <= body.x2) {
            if(peer.y2 < body.y1) area.y1 = LV_MAX(area.y1, (peer.y2 + body.y1) / 2 + 1);
            if(peer.y1 > body.y2) area.y2 = LV_MIN(area.y2, (body.y2 + peer.y1) / 2);
        }
        if(body.y1 <= peer.y2 && peer.y1 <= body.y2) {
            if(peer.x2 < body.x1) area.x1 = LV_MAX(area.x1, (peer.x2 + body.x1) / 2 + 1);
            if(peer.x1 > body.x2) area.x2 = LV_MIN(area.x2, (body.x2 + peer.x1) / 2);
        }
    }
    hit->res = hit->point->x >= area.x1 && hit->point->x <= area.x2 &&
               hit->point->y >= area.y1 && hit->point->y <= area.y2;
}

static lv_obj_t * button(lv_obj_t * parent, int x, int y, int w, int h, const char * text,
                         lv_event_cb_t callback, InstrumentUI * u, lv_obj_t ** out_label)
{
    lv_obj_t * b = lv_button_create(parent); lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h); lv_obj_set_scrollable(b, false);
    /* 先给LVGL一个8像素的搜索外框，再在回调中按相邻按钮收窄，外观大小保持原样。 */
    lv_obj_set_ext_click_area(b, 8);
    lv_obj_set_adv_hittest(b, true);
    lv_obj_add_event_cb(b, button_hit_test, LV_EVENT_HIT_TEST, NULL);
    lv_obj_set_style_radius(b, 4, 0); lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, ui_color(UI_SELECTED), 0);
    lv_obj_set_style_bg_color(b, ui_color(0x598DAB), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, ui_color(UI_BORDER), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(b, ui_color(UI_WHITE), 0);
    lv_obj_set_style_text_color(b, ui_color(UI_UNIT), LV_STATE_DISABLED);
    lv_obj_set_style_pad_all(b, 0, 0);
    if(callback) lv_obj_add_event_cb(b, callback, LV_EVENT_CLICKED, u);
    lv_obj_t * l = label(b, 0, 0, w, &measurement_font_14, UI_WHITE);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, text); lv_obj_center(l);
    if(out_label) *out_label = l;
    return b;
}
static void message(InstrumentUI * u, const char * text)
{
    snprintf(u->message, sizeof(u->message), "%s", text);
    lv_label_set_text(u->footer, text);
}
static bool persist(InstrumentUI * u, const InstrumentModel * before)
{
    if(u->services.save && !u->services.save(u->services.context, u->model)) {
        if(before) *u->model = *before;
        u->storage_failed = true; message(u, "保存失败 · 请检查存储后重试");
        return false;
    }
    u->storage_failed = false;
    return true;
}
void instrument_ui_apply(InstrumentUI * u)
{
    MeasurementPage * p = u->measurement;
    const InstrumentSettings * s = &u->model->settings;
    measurement_page_select_geometry(p, s->mode);
    p->session.timeout_ms = s->timeout_ms;
    measurement_page_set_units(p, s->inches);
    geometry_view_set_options(&p->geometry, s->grid, s->dimensions, s->contour, s->inches);
    bool ready = instrument_calibration_ready(u->model);
    bool calibration_changed = ready != p->session.calibrated ||
        u->applied_calibration_version != u->model->calibration.version;
    measurement_page_set_device(p, p->session.connected, ready);
    if(calibration_changed) measurement_page_invalidate_result(p);
    u->applied_calibration_version = u->model->calibration.version;
}
static bool busy(InstrumentUI * u)
{
    if(measurement_session_busy(&u->measurement->session) || u->capturing) {
        message(u, "当前任务进行中 · 请完成后再修改"); return true;
    }
    return false;
}
bool instrument_ui_set_mode(InstrumentUI * u, MeasurementGeometry mode)
{
    if(!u || busy(u)) return false;
    if(u->model->settings.mode == mode) return measurement_page_select_geometry(u->measurement, mode);
    InstrumentModel *before = &u->rollback; *before = *u->model; InstrumentSettings s = before->settings; s.mode = mode;
    if(!instrument_apply_settings(u->model, &s) || !persist(u, before)) return false;
    measurement_page_select_geometry(u->measurement, mode);
    render(u); return true;
}
static void row(InstrumentUI * u, unsigned i, const char * key, const char * value, bool enabled)
{
    lv_obj_set_hidden(u->rows[i], false);
    lv_obj_set_clickable(u->rows[i], enabled);
    lv_label_set_text(u->row_labels[i], key); lv_label_set_text(u->row_values[i], value);
    lv_obj_set_style_text_color(u->row_values[i], ui_color(enabled ? UI_CYAN : UI_WHITE), 0);
}
static void action(InstrumentUI * u, unsigned i, const char * text, bool enabled)
{
    lv_obj_set_hidden(u->actions[i], false); lv_label_set_text(u->action_labels[i], text);
    lv_obj_set_state(u->actions[i], LV_STATE_DISABLED, !enabled);
}
static void pager(InstrumentUI * u, unsigned index, unsigned total)
{
    bool visible = total > 1;
    lv_obj_set_hidden(u->up, !visible); lv_obj_set_hidden(u->down, !visible); lv_obj_set_hidden(u->page_label, !visible);
    lv_obj_set_state(u->up, LV_STATE_DISABLED, index == 0);
    lv_obj_set_state(u->down, LV_STATE_DISABLED, index + 1 >= total);
    lv_label_set_text_fmt(u->page_label, "%u/%u", index + 1, total);
}

/* 只判断键盘下方新增的空白：point是屏幕坐标，返回对应键号或“没有命中”。
 * 标准数字键盘最后五键依次为+/−、0、.、左、右；左右边界按实际键间隙中线划分。
 * 原键盘底部是Y312，新增区为Y313～319；不扩大左右/上方，不涉及上面三排。 */
static uint32_t keyboard_bottom_key(lv_obj_t * keyboard, const lv_point_t * point)
{
    const lv_buttonmatrix_t * matrix = (const lv_buttonmatrix_t *)keyboard;
    lv_area_t body;
    lv_obj_get_coords(keyboard, &body);
    if(lv_keyboard_get_mode(keyboard) != LV_KEYBOARD_MODE_NUMBER || matrix->btn_cnt < 5U ||
       !matrix->button_areas || point->y <= body.y2 || point->y > 319 ||
       point->x < body.x1 || point->x > body.x2) return LV_BUTTONMATRIX_BUTTON_NONE;
    uint32_t first = matrix->btn_cnt - 5U;
    for(uint32_t i = first; i < matrix->btn_cnt; ++i) {
        int32_t left = i == first ? body.x1 : body.x1 +
            (matrix->button_areas[i - 1U].x2 + matrix->button_areas[i].x1) / 2 + 1;
        int32_t right = i + 1U == matrix->btn_cnt ? body.x2 : body.x1 +
            (matrix->button_areas[i].x2 + matrix->button_areas[i + 1U].x1) / 2;
        if(point->x < left || point->x > right) continue;
        /* 单键禁用或隐藏时，下方空白也不能绕过原限制。 */
        if(lv_buttonmatrix_has_button_ctrl(keyboard, i, LV_BUTTONMATRIX_CTRL_DISABLED) ||
           lv_buttonmatrix_has_button_ctrl(keyboard, i, LV_BUTTONMATRIX_CTRL_HIDDEN))
            return LV_BUTTONMATRIX_BUTTON_NONE;
        return i;
    }
    return LV_BUTTONMATRIX_BUTTON_NONE;
}

/* 矩阵原处理先执行，本回调只补它不能识别的底部空白。
 * 第一次按下时选中对应键，发出矩阵正常的VALUE_CHANGED（键值变化）事件，
 * 仍由LVGL键盘处理字符、正负号和光标；不复制输入逻辑、不手工制造CLICKED。
 * 按住期间恢复同一个键的选择，让原有长按重复继续工作；横移到别列则取消本次按键。 */
static void keyboard_bottom_event(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e);
    lv_obj_t * keyboard = lv_event_get_target_obj(e);
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t * hit = lv_event_get_hit_test_info(e);
        lv_area_t body;
        lv_obj_get_coords(keyboard, &body);
        hit->res = (hit->point->x >= body.x1 && hit->point->x <= body.x2 &&
                    hit->point->y >= body.y1 && hit->point->y <= body.y2) ||
                   keyboard_bottom_key(keyboard, hit->point) != LV_BUTTONMATRIX_BUTTON_NONE;
        return;
    }
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        u->keyboard_pressed_key = LV_BUTTONMATRIX_BUTTON_NONE;
        return;
    }
    if(code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;
    lv_indev_t * input = lv_event_get_indev(e);
    if(!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER || lv_obj_is_hidden(u->editor)) return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    uint32_t key = keyboard_bottom_key(keyboard, &point);
    if(code == LV_EVENT_PRESSED) {
        u->keyboard_pressed_key = lv_buttonmatrix_get_selected_button(keyboard);
        if(key == LV_BUTTONMATRIX_BUTTON_NONE) return;
        u->keyboard_pressed_key = key;
        lv_buttonmatrix_set_selected_button(keyboard, key);
        /* 标准底行五键均在按下时输入一次；后续重复由矩阵自身产生。 */
        (void)lv_obj_send_event(keyboard, LV_EVENT_VALUE_CHANGED, &key);
    }
    else if(key != LV_BUTTONMATRIX_BUTTON_NONE && key == u->keyboard_pressed_key) {
        lv_buttonmatrix_set_selected_button(keyboard, key);
    }
    else {
        u->keyboard_pressed_key = lv_buttonmatrix_get_selected_button(keyboard);
    }
}

static void show_editor(InstrumentUI * u, InstrumentEdit edit, const char * title, const char * hint, const char * value)
{
    u->edit = edit; lv_label_set_text(u->editor_title, title); lv_label_set_text(u->editor_hint, hint);
    lv_obj_set_style_text_color(u->editor_hint, ui_color(UI_UNIT), 0);
    lv_textarea_set_text(u->textarea, value); lv_obj_set_hidden(u->editor, false);
    lv_keyboard_set_textarea(u->keyboard, u->textarea);
    lv_obj_add_state(u->textarea, LV_STATE_FOCUSED);
}
static void close_editor(InstrumentUI * u)
{
    u->edit = EDIT_NONE; lv_keyboard_set_textarea(u->keyboard, NULL); lv_obj_set_hidden(u->editor, true);
    lv_obj_remove_state(u->textarea, LV_STATE_FOCUSED);
}
static void keyboard_event(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e);
    if(lv_event_get_code(e) == LV_EVENT_CANCEL) { close_editor(u); return; }
    if(lv_event_get_code(e) != LV_EVENT_READY) return;
    const char * text = lv_textarea_get_text(u->textarea); char * end;
    double value = strtod(text, &end);
    double min = u->edit == EDIT_REFERENCE ? 1 : u->edit == EDIT_SAMPLES ? 32 : u->edit == EDIT_QUALITY ? 0.01 : 2;
    double max = u->edit == EDIT_REFERENCE ? 200 : u->edit == EDIT_SAMPLES ? 512 : u->edit == EDIT_QUALITY ? 1 : 30;
    bool integer = u->edit == EDIT_SAMPLES || u->edit == EDIT_TIMEOUT;
    double scaled = value * ((u->edit == EDIT_REFERENCE || u->edit == EDIT_QUALITY) ? 100 : 1);
    if(!*text || *end || !isfinite(value) || value < min || value > max ||
       (integer && floor(value) != value) || fabs(scaled - round(scaled)) > 1e-6) {
        lv_label_set_text(u->editor_hint, "输入无效 · 请按范围输入正确数值");
        lv_obj_set_style_text_color(u->editor_hint, ui_color(UI_YELLOW), 0); return;
    }
    if(busy(u)) return;
    if(u->edit == EDIT_REFERENCE) u->reference_cmm = (uint32_t)lround(scaled);
    else {
        InstrumentModel *before = &u->rollback; *before = *u->model; InstrumentSettings s = before->settings;
        if(u->edit == EDIT_SAMPLES) s.sample_count = (uint32_t)value;
        if(u->edit == EDIT_QUALITY) s.quality_cmm = (uint32_t)lround(scaled);
        if(u->edit == EDIT_TIMEOUT) s.timeout_ms = (uint32_t)value * 1000;
        instrument_apply_settings(u->model, &s);
        if(!persist(u, before)) { lv_label_set_text(u->editor_hint, u->message); return; }
        instrument_ui_apply(u);
    }
    close_editor(u); message(u, "参数已更新"); render(u);
}
static void confirm_cancel(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e); u->confirm = CONFIRM_NONE; lv_obj_set_hidden(u->confirmation, true);
}
static void show_confirm(InstrumentUI * u, InstrumentConfirm confirm, const char * text)
{
    u->confirm = confirm; lv_label_set_text(u->confirmation_text, text); lv_obj_set_hidden(u->confirmation, false);
}
static void confirm_accept(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e);
    if((u->confirm == CONFIRM_DEFAULTS || u->confirm == CONFIRM_PROFILE) && busy(u)) return;
    InstrumentModel *before = &u->rollback; *before = *u->model;
    if(u->confirm == CONFIRM_DELETE) instrument_delete_record(u->model, u->selected_record);
    else if(u->confirm == CONFIRM_CLEAR) { u->model->record_count = 0; memset(u->model->records, 0, sizeof(u->model->records)); }
    else if(u->confirm == CONFIRM_DEFAULTS) {
        InstrumentSettings defaults; instrument_default_settings(&defaults); instrument_apply_settings(u->model, &defaults);
    }
    else if(u->confirm == CONFIRM_PROFILE) {
        InstrumentSettings s = u->model->settings; s.image_profile = !s.image_profile; instrument_apply_settings(u->model, &s);
    }
    if(!persist(u, before)) {
        lv_label_set_text(u->confirmation_text, "保存失败\n检查存储后可再次确认"); return;
    }
    u->selected_record = 0; u->records_page = 0;
    confirm_cancel(e); instrument_ui_apply(u); message(u, u->services.persistent ? "操作已保存" : "操作已更新 / 仅内存"); render(u);
}
static void row_click(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e); unsigned i = 0;
    while(i < 5 && u->rows[i] != lv_event_get_target_obj(e)) ++i;
    if(i >= 5) return;
    unsigned page = u->measurement->selected_navigation; char text[40];
    if(page == 2) {
        unsigned index = u->records_page * 4 + i;
        if(!u->selected_record && index < u->model->record_count) { u->selected_record = u->model->records[index].id; message(u, "历史记录 · 尺寸单位随系统设置显示"); render(u); }
        return;
    }
    if(page == 3 && u->settings_page == SETTINGS_MENU) {
        if(i < 3) {
            u->settings_page = (InstrumentSettingsPage)(SETTINGS_MEASUREMENT + i);
            message(u, i == 2 ? "设备状态与存储诊断" :
                u->services.persistent ? "点击青色选项修改 · 自动保存" : "点击青色选项修改 / 仅内存"); render(u);
        }
        return;
    }
    if(busy(u)) return;
    if(page == 1 && u->wizard && u->calibration_step == 0 && i == 0) {
        measurement_format_cmm(text, sizeof(text), u->reference_cmm);
        show_editor(u, EDIT_REFERENCE, "标准参考边长 (mm)", "范围 1.00~200.00 mm · 最多两位小数", text); return;
    }
    if(page != 3) return;
    InstrumentModel *before = &u->rollback; *before = *u->model; InstrumentSettings s = before->settings;
    if(u->settings_page == SETTINGS_MEASUREMENT) {
        if(i == 0) s.mode = s.mode == MEASUREMENT_RING ? MEASUREMENT_RECTANGLE : MEASUREMENT_RING;
        if(i == 1) { snprintf(text, sizeof(text), "%" PRIu32, s.sample_count); show_editor(u, EDIT_SAMPLES, "采样点数", "范围 32~512 · 请输入整数", text); return; }
        if(i == 2) { measurement_format_cmm(text, sizeof(text), s.quality_cmm); show_editor(u, EDIT_QUALITY, "质量阈值 (mm)", "拟合残差上限 0.01~1.00 mm", text); return; }
        if(i == 3) s.inches = !s.inches;
        if(i == 4) { snprintf(text, sizeof(text), "%" PRIu32, s.timeout_ms / 1000); show_editor(u, EDIT_TIMEOUT, "测量超时 (s)", "范围 2~30 s · 请输入整数", text); return; }
    }
    else if(u->settings_page == SETTINGS_DISPLAY) {
        if(i == 0) s.grid = !s.grid;
        if(i == 1) s.dimensions = !s.dimensions;
        if(i == 2) s.contour = !s.contour;
        if(i == 3) { show_confirm(u, CONFIRM_PROFILE, "更改成像配置？\n当前标定将失效，需要重新标定"); return; }
        if(i == 4) return;
    }
    else return;
    instrument_apply_settings(u->model, &s);
    if(persist(u, before)) { instrument_ui_apply(u); message(u, u->services.persistent ? "设置已保存" : "设置已更新 / 仅内存"); }
    render(u);
}
static void calibration_tick(lv_timer_t * timer)
{
    InstrumentUI * u = lv_timer_get_user_data(timer);
    if(!u->capturing) { lv_timer_pause(timer); return; }
    if(!u->measurement->session.connected) {
        u->capturing = false; measurement_page_set_calibration_capture(u->measurement, false);
        lv_timer_pause(timer); message(u, "相机未连接 · 恢复连接后重新采集"); render(u); return;
    }
    if(lv_tick_get() - u->capture_started < 1200) return;
    u->capturing = false; measurement_page_set_calibration_capture(u->measurement, false); lv_timer_pause(timer);
    InstrumentPoint points[4], verification; uint32_t expected;
    if(!u->services.capture_calibration ||
       !u->services.capture_calibration(u->services.calibration_context, u->model->settings.image_profile, points, &verification, &expected) ||
       !instrument_calculate_calibration(points, u->reference_cmm, verification, expected, u->model->settings.image_profile, &u->candidate)) {
        message(u, "采集或计算失败 · 请检查参考特征后重试"); render(u); return;
    }
    u->calibration_step = 2;
    message(u, u->candidate.valid ? "验证通过 · 保存后生效" : "验证失败 · 请确认标准参考边长"); render(u);
}
static void action_click(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e); unsigned i = lv_event_get_target_obj(e) == u->actions[1];
    unsigned page = u->measurement->selected_navigation;
    if(page == 1) {
        if(busy(u)) return;
        if(!u->wizard) {
            if(i == 0) { u->wizard = true; u->calibration_step = 0; u->reference_cmm = 5000; message(u, "模拟测量使用 50.00 mm 方形参考件"); }
            else {
                InstrumentCalibration loaded;
                if(!u->services.load_calibration || !u->services.load_calibration(u->services.context, &loaded) ||
                   !loaded.valid || loaded.image_profile != u->model->settings.image_profile) message(u, "没有匹配当前成像配置的已存标定");
                else { u->model->calibration = loaded; instrument_ui_apply(u); message(u, "已加载保存的标定参数"); }
            }
        }
        else if(i == 1) {
            if(u->calibration_step > 0) { --u->calibration_step; message(u, "可调整参考尺寸后重新采集"); }
            else { u->wizard = false; message(u, "标定向导已取消"); }
        }
        else if(u->calibration_step == 0) { u->calibration_step = 1; message(u, "参考件保持平整 · 点击采集四个角点"); }
        else if(u->calibration_step == 1) {
            if(!u->measurement->session.connected) message(u, "相机未连接 · 无法采集");
            else { u->capturing = true; measurement_page_set_calibration_capture(u->measurement, true);
                u->capture_started = lv_tick_get(); lv_timer_reset(u->calibration_timer); lv_timer_resume(u->calibration_timer); message(u, "正在采集模拟角点与独立验证特征..."); }
        }
        else if(u->candidate.valid) {
            InstrumentModel *before = &u->rollback; *before = *u->model;
            if(before->calibration.version == UINT32_MAX) { message(u, "标定版本已达上限"); return; }
            u->model->calibration = u->candidate; u->model->calibration.version = before->calibration.version + 1;
            if(persist(u, before)) { u->wizard = false; instrument_ui_apply(u); message(u, u->services.persistent ? "标定参数已保存 · 测量已就绪" : "标定已更新 / 仅内存 / 模拟测量"); }
        }
    }
    else if(page == 2) {
        if(u->selected_record) {
            if(i == 0) { u->selected_record = 0; message(u, "选择记录查看详情"); }
            else show_confirm(u, CONFIRM_DELETE, "删除这条测量记录？\n删除后无法恢复");
        }
        else if(i == 0 && !u->model->record_count) { measurement_page_navigate(u->measurement, 0); return; }
        else if(i == 1) show_confirm(u, CONFIRM_CLEAR, "清空全部测量记录？\n此操作只清空测量历史");
    }
    else if(page == 3 && u->settings_page == SETTINGS_DEVICE && !busy(u)) show_confirm(u, CONFIRM_DEFAULTS, "恢复默认设置？\n历史记录保留，成像变化将使标定失效");
    render(u);
}
static void page_click(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e); bool down = lv_event_get_target_obj(e) == u->down;
    unsigned * current = &u->records_page;
    unsigned total = (u->model->record_count + 3) / 4;
    if(down && *current + 1 < total) ++*current;
    if(!down && *current > 0) --*current;
    message(u, "选择记录查看详情"); render(u);
}
static void settings_back(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e);
    u->settings_page = SETTINGS_MENU;
    message(u, u->services.persistent ? "设置修改后自动保存" : "设置修改后仅在内存"); render(u);
}
static void draw_preview(lv_event_t * e)
{
    InstrumentUI * u = lv_event_get_user_data(e); lv_area_t a; lv_obj_get_coords(u->preview, &a);
    lv_layer_t * l = lv_event_get_layer(e); int x = a.x1, y = a.y1;
    ui_rect(l, x+40, y+13, 114, 88, UI_PLOT, LV_OPA_COVER, 0, UI_CYAN, 2);
    for(unsigned i = 0; i < 4; ++i) {
        int px = x + ((i == 1 || i == 2) ? 153 : 40), py = y + (i >= 2 ? 100 : 13);
        ui_rect(l, px-4, py-4, 9, 9, u->capturing ? UI_YELLOW : UI_CYAN, LV_OPA_COVER, LV_RADIUS_CIRCLE, 0, 0);
    }
    int text_width = lv_obj_get_width(u->preview) - 200;
    ui_text(l, x+188, y+16, text_width, &measurement_font_14, UI_WHITE, "四角点平面映射", LV_TEXT_ALIGN_LEFT);
    ui_text(l, x+188, y+49, text_width, &measurement_font_12, UI_UNIT, "采集 4 点 + 独立 20 mm 验证", LV_TEXT_ALIGN_LEFT);
    ui_text(l, x+188, y+78, text_width, &measurement_font_12, UI_YELLOW, "模拟测量 · 镜头畸变未校正", LV_TEXT_ALIGN_LEFT);
    ui_text(l, x+34, y+110, 150, &measurement_font_12, UI_UNIT, u->capturing ? "采集中..." : "标准方形参考件", LV_TEXT_ALIGN_CENTER);
}
static void render_calibration(InstrumentUI * u)
{
    char v[96], n[24]; const InstrumentCalibration * c = &u->model->calibration;
    lv_label_set_text(u->title, u->wizard ? "标定向导" : "相机标定");
    if(!u->wizard) {
        lv_label_set_text(u->subtitle, "像素坐标 → 测量平面 · 模拟测量");
        row(u, 0, "当前状态", instrument_calibration_ready(u->model) ? "已标定" : c->version ? "标定已失效" : "未标定", false);
        snprintf(v, sizeof(v), "V%" PRIu32 " / %s", c->version, u->model->settings.image_profile ? "1280x960" : "640x480"); row(u, 1, "版本 / 成像配置", v, false);
        measurement_format_cmm(n, sizeof(n), c->reference_cmm); snprintf(v, sizeof(v), "%s mm", c->version ? n : "--"); row(u, 2, "标准参考边长", v, false);
        measurement_format_cmm(n, sizeof(n), c->error_cmm); snprintf(v, sizeof(v), "%s mm", c->version ? n : "--"); row(u, 3, "独立验证误差", v, false);
        row(u, 4, "映射 / 畸变", "平面单应性 / 未校正", false);
        action(u, 0, c->version ? "重新标定" : "开始标定", !measurement_session_busy(&u->measurement->session));
        action(u, 1, "加载已存参数", true);
    }
    else {
        static const char * const steps[] = {"1 参考尺寸    ·    2 采集特征    ·    3 验证保存", "1 参考尺寸    →    2 采集特征    ·    3 验证保存", "1 参考尺寸    ·    2 采集特征    →    3 验证保存"};
        lv_label_set_text(u->subtitle, steps[u->calibration_step]);
        if(u->calibration_step == 0) {
            measurement_format_cmm(n, sizeof(n), u->reference_cmm); snprintf(v, sizeof(v), "%s mm  >", n);
            row(u, 0, "标准参考边长", v, true); row(u, 1, "操作 1", "放置 50.00 mm 模拟参考件", false);
            row(u, 2, "操作 2", "保持平面与照明稳定", false); row(u, 3, "验证标准", "独立 20 mm / 误差 ≤0.50 mm", false);
            row(u, 4, "说明", "仅模拟平面标定，不含畸变校正", false);
            action(u, 0, "下一步", true); action(u, 1, "取消", true);
        }
        else if(u->calibration_step == 1) {
            lv_obj_set_hidden(u->preview, false); lv_obj_invalidate(u->preview);
            action(u, 0, u->capturing ? "采集中..." : "采集参考特征", !u->capturing);
            action(u, 1, "上一步", !u->capturing);
        }
        else {
            c = &u->candidate;
            row(u, 0, "验证结果", c->valid ? "通过 · 可保存" : "失败 · 请重新采集", false);
            measurement_format_cmm(n, sizeof(n), c->error_cmm); snprintf(v, sizeof(v), "%s mm / 上限 0.50 mm", n); row(u, 1, "独立验证误差", v, false);
            snprintf(v, sizeof(v), "%.6f mm/px", c->homography[0]); row(u, 2, "X 方向比例", v, false);
            snprintf(v, sizeof(v), "%.6f mm/px", c->homography[4]); row(u, 3, "Y 方向比例", v, false);
            snprintf(v, sizeof(v), "V%" PRIu32 " · 模拟测量", u->model->calibration.version + 1); row(u, 4, "保存版本", v, false);
            action(u, 0, "保存并应用", c->valid); action(u, 1, "重新采集", true);
        }
    }
}
static void render_records(InstrumentUI * u)
{
    char key[96], value[96], a[24], b[24]; bool inches = u->model->settings.inches; const char * unit = inches ? "in" : "mm";
    lv_label_set_text(u->title, u->selected_record ? "记录详情" : "测量记录");
    if(u->selected_record) {
        const InstrumentRecord * r = NULL;
        for(unsigned i = 0; i < u->model->record_count; ++i) if(u->model->records[i].id == u->selected_record) r = u->model->records + i;
        if(!r) { u->selected_record = 0; render_records(u); return; }
        lv_label_set_text_fmt(u->subtitle, "历史结果 #%06" PRIu32 " · %s", r->id, r->result.simulated ? "模拟测量" : "设备采集");
        row(u, 0, "工件类型 / 有效性", r->result.geometry == MEASUREMENT_RING ? (r->result.status == MEASUREMENT_VALID ? "圆环 / 有效" : "圆环 / 无效") : (r->result.status == MEASUREMENT_VALID ? "矩形 / 有效" : "矩形 / 无效"), false);
        instrument_format_dimension(a, sizeof(a), (uint32_t)r->result.primary_cmm, inches); snprintf(value, sizeof(value), "%s %s", a, unit); row(u, 1, r->result.geometry == MEASUREMENT_RING ? "外径" : "长度", value, false);
        instrument_format_dimension(a, sizeof(a), (uint32_t)r->result.secondary_cmm, inches); snprintf(value, sizeof(value), "%s %s", a, unit); row(u, 2, r->result.geometry == MEASUREMENT_RING ? "内径" : "宽度", value, false);
        measurement_format_cmm(a, sizeof(a), r->result.residual_cmm); snprintf(value, sizeof(value), "%s mm", a); row(u, 3, "拟合残差", value, false);
        snprintf(value, sizeof(value), "V%" PRIu32 " / 未配置公差判定", r->calibration_version); row(u, 4, "标定版本 / 公差", value, false);
        action(u, 0, "返回列表", true); action(u, 1, "删除此记录", true); return;
    }
    unsigned total = (u->model->record_count + 3) / 4;
    if(total && u->records_page >= total) u->records_page = total - 1;
    lv_label_set_text_fmt(u->subtitle, "共 %" PRIu32 " 条 / 最多 64 条 / %s", u->model->record_count, u->services.persistent ? "自动保存" : "仅内存");
    if(!total) {
        row(u, 0, "暂无测量记录", (u->services.persistent ? "完成测量后自动保存" : "完成测量后仅在内存"), false);
        lv_obj_set_pos(u->rows[0], 8, 66); lv_obj_set_height(u->rows[0], 76);
        lv_obj_set_style_bg_opa(u->rows[0], LV_OPA_TRANSP, 0);
        lv_obj_set_pos(u->row_labels[0], 0, 9); lv_obj_set_width(u->row_labels[0], CONTENT_WIDTH);
        lv_obj_set_style_text_font(u->row_labels[0], &measurement_font_16_bold, 0);
        lv_obj_set_style_text_align(u->row_labels[0], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(u->row_values[0], 0, 37); lv_obj_set_width(u->row_values[0], CONTENT_WIDTH);
        lv_obj_set_style_text_color(u->row_values[0], ui_color(UI_UNIT), 0);
        lv_obj_set_style_text_align(u->row_values[0], LV_TEXT_ALIGN_CENTER, 0);
        action(u, 0, "开始测量", true);
        lv_obj_set_pos(u->actions[0], (PANEL_WIDTH - 200) / 2, 156); lv_obj_set_size(u->actions[0], 200, 30);
        lv_obj_set_width(u->action_labels[0], 200); lv_obj_center(u->action_labels[0]);
        message(u, u->storage_failed ? "存储异常 · 请检查存储" : "前往测量页面开始一次测量"); return;
    }
    for(unsigned i = 0; i < 4 && u->records_page * 4 + i < u->model->record_count; ++i) {
        const InstrumentRecord * r = u->model->records + u->records_page * 4 + i;
        instrument_format_dimension(a, sizeof(a), (uint32_t)r->result.primary_cmm, inches);
        instrument_format_dimension(b, sizeof(b), (uint32_t)r->result.secondary_cmm, inches);
        char residual[20]; measurement_format_cmm(residual, sizeof(residual), r->result.residual_cmm);
        snprintf(key, sizeof(key), "#%06" PRIu32 " %s · %s · 残差 %s mm", r->id, r->result.geometry == MEASUREMENT_RING ? "圆环" : "矩形", r->result.status == MEASUREMENT_VALID ? "有效" : "无效", residual);
        snprintf(value, sizeof(value), "%s / %s %s  >", a, b, unit); row(u, i, key, value, true);
        lv_obj_set_pos(u->rows[i], 8, 46 + (int)i * 32); lv_obj_set_height(u->rows[i], 30);
        lv_obj_set_pos(u->row_labels[i], 10, 0); lv_obj_set_width(u->row_labels[i], CONTENT_WIDTH - 20);
        lv_obj_set_pos(u->row_values[i], 10, 14); lv_obj_set_width(u->row_values[i], CONTENT_WIDTH - 20);
        lv_obj_set_style_text_align(u->row_values[i], LV_TEXT_ALIGN_LEFT, 0);
    }
    action(u, 1, "清空记录", true);
    lv_obj_set_pos(u->actions[1], 303, 178); lv_obj_set_width(u->actions[1], 152);
    lv_obj_set_width(u->action_labels[1], 152); lv_obj_center(u->action_labels[1]);
    pager(u, u->records_page, total);
}
static void render_settings(InstrumentUI * u)
{
    static const char * const titles[] = {"测量参数", "显示设置", "设备与系统"};
    char value[96], number[24]; const InstrumentSettings * s = &u->model->settings;
    if(u->settings_page == SETTINGS_MENU) {
        lv_label_set_text(u->title, "系统设置"); lv_label_set_text(u->subtitle, "点击选项进入设置");
        for(unsigned i = 0; i < 3; ++i) {
            row(u, i, titles[i], ">", true);
            lv_obj_set_pos(u->rows[i], 8, 48 + (int)i * 51); lv_obj_set_height(u->rows[i], 45);
            lv_obj_set_pos(u->row_labels[i], 14, 13); lv_obj_set_width(u->row_labels[i], CONTENT_WIDTH - 52);
            lv_obj_set_style_text_font(u->row_labels[i], &measurement_font_14, 0);
            lv_obj_set_style_text_color(u->row_labels[i], ui_color(UI_WHITE), 0);
            lv_obj_set_pos(u->row_values[i], CONTENT_WIDTH - 34, 13); lv_obj_set_width(u->row_values[i], 20);
            lv_obj_set_style_text_font(u->row_values[i], &measurement_font_14, 0);
        }
        return;
    }
    lv_obj_set_hidden(u->back, false);
    lv_obj_set_pos(u->title, 88, 6); lv_obj_set_width(u->title, PANEL_WIDTH - 100);
    lv_obj_set_pos(u->subtitle, 88, 28); lv_obj_set_width(u->subtitle, PANEL_WIDTH - 100);
    lv_label_set_text(u->title, titles[u->settings_page - SETTINGS_MEASUREMENT]);
    lv_label_set_text(u->subtitle, u->services.persistent ? "点击青色选项修改 · 自动保存" : "点击青色选项修改 / 仅内存");
    for(unsigned i = 0; i < 5; ++i) {
        bool device = u->settings_page == SETTINGS_DEVICE;
        lv_obj_set_pos(u->rows[i], 8, 46 + (int)i * (device ? 26 : 31));
        lv_obj_set_height(u->rows[i], device ? 24 : 28);
        lv_obj_set_pos(u->row_labels[i], 10, device ? 4 : 5); lv_obj_set_width(u->row_labels[i], 132);
        lv_obj_set_pos(u->row_values[i], 152, device ? 4 : 5); lv_obj_set_width(u->row_values[i], CONTENT_WIDTH - 162);
        lv_obj_set_style_text_align(u->row_values[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_font(u->row_labels[i], device ? &measurement_font_12 : &measurement_font_14, 0);
        lv_obj_set_style_text_font(u->row_values[i], device ? &measurement_font_12 : &measurement_font_14, 0);
    }
    if(u->settings_page == SETTINGS_MEASUREMENT) {
        row(u, 0, "测量模式", s->mode == MEASUREMENT_RING ? "圆环  >" : "矩形  >", true);
        snprintf(value, sizeof(value), "%" PRIu32 " 点  >", s->sample_count); row(u, 1, "采样点数", value, true);
        measurement_format_cmm(number, sizeof(number), s->quality_cmm); snprintf(value, sizeof(value), "%s mm  >", number); row(u, 2, "拟合残差上限", value, true);
        row(u, 3, "尺寸单位", s->inches ? "in  >" : "mm  >", true);
        snprintf(value, sizeof(value), "%" PRIu32 " s  >", s->timeout_ms / 1000); row(u, 4, "测量超时", value, true);
    }
    else if(u->settings_page == SETTINGS_DISPLAY) {
        row(u, 0, "网格显示", s->grid ? "开启  >" : "关闭  >", true);
        row(u, 1, "尺寸标注", s->dimensions ? "开启  >" : "关闭  >", true);
        row(u, 2, "工件轮廓", s->contour ? "开启  >" : "关闭  >", true);
        row(u, 3, "成像配置", s->image_profile ? "1280x960  >" : "640x480  >", true);
        row(u, 4, "屏幕亮度", "当前不支持背光调节", false);
    }
    else {
        lv_label_set_text(u->subtitle, "连接 · 标定 · 存储状态");
        row(u, 0, "相机连接", u->measurement->session.connected ? "已连接 · 模拟测量" : "未连接 · 模拟测量", false);
        row(u, 1, "通信状态", u->measurement->session.connected ? "模拟事件 / UART 待接入" : "连接中断", false);
        row(u, 2, "固件 / 协议", "F407 / LVGL 9.6 / UART 未实现", false);
        row(u, 3, "当前标定", instrument_calibration_ready(u->model) ? "已标定 · 平面映射" : "未标定 / 已失效", false);
        row(u, 4, "存储诊断", u->storage_failed ? "保存失败 · 检查存储" : (u->services.persistent ? (u->services.storage_name ? u->services.storage_name : "设备存储") : "仅内存 / Flash 未接入"), false);
        action(u, 1, "恢复默认设置", true);
    }
}
static void render(InstrumentUI * u)
{
    unsigned page = u->measurement->selected_navigation;
    lv_obj_set_hidden(u->panel, page == 0); if(page == 0) return;
    lv_obj_set_hidden(u->back, true);
    lv_obj_set_pos(u->title, 12, 6); lv_obj_set_width(u->title, PANEL_WIDTH - 24);
    lv_obj_set_pos(u->subtitle, 12, 28); lv_obj_set_width(u->subtitle, PANEL_WIDTH - 24);
    for(unsigned i = 0; i < 5; ++i) {
        lv_obj_set_hidden(u->rows[i], true);
        lv_obj_set_style_bg_opa(u->rows[i], LV_OPA_COVER, 0);
        lv_obj_set_pos(u->rows[i], 8, 46 + (int)i * 26); lv_obj_set_height(u->rows[i], 24);
        lv_obj_set_pos(u->row_labels[i], 8, 4); lv_obj_set_width(u->row_labels[i], 168);
        lv_obj_set_style_text_font(u->row_labels[i], &measurement_font_12, 0);
        lv_obj_set_style_text_color(u->row_labels[i], ui_color(UI_MUTED), 0);
        lv_obj_set_style_text_align(u->row_labels[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_pos(u->row_values[i], 175, 4); lv_obj_set_width(u->row_values[i], CONTENT_WIDTH - 183);
        lv_obj_set_style_text_font(u->row_values[i], &measurement_font_12, 0);
        lv_obj_set_style_text_align(u->row_values[i], LV_TEXT_ALIGN_RIGHT, 0);
    }
    for(unsigned i = 0; i < 2; ++i) {
        lv_obj_set_hidden(u->actions[i], true);
        lv_obj_set_pos(u->actions[i], i ? 238 : 8, 178); lv_obj_set_size(u->actions[i], 217, 27);
        lv_obj_set_width(u->action_labels[i], 217); lv_obj_center(u->action_labels[i]);
    }
    lv_obj_set_hidden(u->preview, true); pager(u, 0, 1);
    lv_label_set_text(u->footer, u->message);
    if(page == 1) render_calibration(u);
    if(page == 2) render_records(u);
    if(page == 3) render_settings(u);
}
static void navigation_changed(void * context, unsigned page)
{
    InstrumentUI * u = context;
    if(page == 2) { u->selected_record = 0; u->records_page = 0; }
    if(page == 3) u->settings_page = SETTINGS_MENU;
    if(!u->storage_failed) message(u, !u->services.persistent ? "模拟测量 / 仅内存 / 不保存" : page == 1 ? "标定参数自动恢复 · 可开始向导" : page == 2 ? "选择记录查看详情" : "设置修改后自动保存");
    render(u);
}
static void result_completed(void * context, const MeasurementResult * result)
{
    InstrumentUI * u = context;
    /* Completed outcomes (including invalid quality) are recorded once; failed
     * or late transport requests never enter this callback. */
    if(!instrument_add_record(u->model, result)) { message(u, "记录序号已达上限 · 无法保存新记录"); return; }
    if(!persist(u, NULL)) {
        message(u, "本次记录仅在内存 · 存储失败请检查存储");
        measurement_page_show_notice(u->measurement, "记录未保存 · 检查存储", true);
    }
    else message(u, u->services.persistent ? "本次测量记录已保存" : "本次记录仅在内存 / 不保存");
    render(u);
}
void instrument_ui_refresh(InstrumentUI * u) { if(u) render(u); }
bool instrument_ui_input_active(const InstrumentUI * u) { return u && (u->edit != EDIT_NONE || u->confirm != CONFIRM_NONE); }
void instrument_ui_destroy(InstrumentUI * u)
{
    if(u && u->calibration_timer) { lv_timer_delete(u->calibration_timer); u->calibration_timer = NULL; }
}
void instrument_ui_create(InstrumentUI * u, MeasurementPage * p, InstrumentModel * m,
                          const InstrumentServices * services, const char * startup)
{
    memset(u, 0, sizeof(*u));
    u->measurement = p; u->model = m; u->services = *services; u->reference_cmm = 5000;
    u->panel = box(p->root, 7, 43, PANEL_WIDTH, 229, UI_PANEL);
    u->title = label(u->panel, 12, 6, PANEL_WIDTH - 24, &measurement_font_16_bold, UI_WHITE);
    u->subtitle = label(u->panel, 12, 28, PANEL_WIDTH - 24, &measurement_font_12, UI_UNIT);
    u->back = button(u->panel, 8, 6, 68, 30, "返回", settings_back, u, NULL);
    for(unsigned i = 0; i < 5; ++i) {
        u->rows[i] = button(u->panel, 8, 46 + (int)i * 26, CONTENT_WIDTH, 24, "", row_click, u, NULL);
        lv_obj_set_style_bg_color(u->rows[i], ui_color(UI_PLOT), 0);
        u->row_labels[i] = label(u->rows[i], 8, 4, 168, &measurement_font_12, UI_MUTED);
        u->row_values[i] = label(u->rows[i], 175, 4, CONTENT_WIDTH - 183, &measurement_font_12, UI_CYAN);
        lv_obj_set_style_text_align(u->row_values[i], LV_TEXT_ALIGN_RIGHT, 0);
    }
    u->preview = box(u->panel, 8, 49, CONTENT_WIDTH, 126, UI_PLOT);
    lv_obj_add_event_cb(u->preview, draw_preview, LV_EVENT_DRAW_MAIN, u);
    u->actions[0] = button(u->panel, 8, 178, 217, 27, "", action_click, u, &u->action_labels[0]);
    u->actions[1] = button(u->panel, 238, 178, 217, 27, "", action_click, u, &u->action_labels[1]);
    u->footer = label(u->panel, 8, 209, CONTENT_WIDTH, &measurement_font_12, UI_UNIT);
    u->up = button(u->panel, 8, 178, 42, 27, "", page_click, u, NULL);
    u->down = button(u->panel, 110, 178, 42, 27, "", page_click, u, NULL);
    lv_obj_t * arrow = label(u->up, 0, 0, 42, &lv_font_montserrat_14, UI_WHITE);
    lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0); lv_label_set_text(arrow, LV_SYMBOL_LEFT); lv_obj_center(arrow);
    arrow = label(u->down, 0, 0, 42, &lv_font_montserrat_14, UI_WHITE);
    lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0); lv_label_set_text(arrow, LV_SYMBOL_RIGHT); lv_obj_center(arrow);
    u->page_label = label(u->panel, 54, 183, 52, &measurement_font_12, UI_UNIT); lv_obj_set_style_text_align(u->page_label, LV_TEXT_ALIGN_CENTER, 0);
    u->editor = box(p->root, 0, 0, 480, 320, UI_BG); lv_obj_set_clickable(u->editor, true);
    u->editor_title = label(u->editor, 14, 9, 452, &measurement_font_16_bold, UI_WHITE);
    u->textarea = lv_textarea_create(u->editor); lv_obj_set_pos(u->textarea, 14, 39); lv_obj_set_size(u->textarea, 452, 45);
    lv_textarea_set_one_line(u->textarea, true); lv_textarea_set_max_length(u->textarea, 9);
    lv_textarea_set_accepted_chars(u->textarea, "0123456789."); lv_obj_set_style_text_font(u->textarea, &measurement_font_24_bold, 0);
    lv_obj_set_style_bg_color(u->textarea, ui_color(UI_PANEL), 0); lv_obj_set_style_text_color(u->textarea, ui_color(UI_WHITE), 0);
    lv_obj_set_style_border_color(u->textarea, ui_color(UI_CYAN), 0);
    u->editor_hint = label(u->editor, 14, 92, 452, &measurement_font_12, UI_UNIT);
    u->keyboard = lv_keyboard_create(u->editor); lv_obj_set_align(u->keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(u->keyboard, 7, 121); lv_obj_set_size(u->keyboard, 466, 192);
    lv_keyboard_set_mode(u->keyboard, LV_KEYBOARD_MODE_NUMBER);
    /* 外观仍到Y312，仅扩大命中搜索；回调把下面7行空白分别交给最下面五键。 */
    lv_obj_set_ext_click_area(u->keyboard, 7);
    lv_obj_set_adv_hittest(u->keyboard, true);
    u->keyboard_pressed_key = LV_BUTTONMATRIX_BUTTON_NONE;
    lv_obj_add_event_cb(u->keyboard, keyboard_bottom_event, LV_EVENT_ALL, u);
    lv_obj_set_style_bg_color(u->keyboard, ui_color(UI_BG), 0); lv_obj_set_style_bg_color(u->keyboard, ui_color(UI_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_text_color(u->keyboard, ui_color(UI_WHITE), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(u->keyboard, ui_color(UI_SELECTED), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(u->keyboard, keyboard_event, LV_EVENT_READY, u); lv_obj_add_event_cb(u->keyboard, keyboard_event, LV_EVENT_CANCEL, u);
    lv_obj_set_hidden(u->editor, true);
    u->confirmation = box(p->root, 0, 0, 480, 320, UI_BG); lv_obj_set_clickable(u->confirmation, true);
    box(u->confirmation, 32, 73, 416, 168, UI_PANEL);
    u->confirmation_text = label(u->confirmation, 48, 94, 384, &measurement_font_14, UI_WHITE);
    lv_label_set_long_mode(u->confirmation_text, LV_LABEL_LONG_WRAP);
    button(u->confirmation, 48, 184, 177, 38, "取消", confirm_cancel, u, NULL);
    u->confirm_button = button(u->confirmation, 255, 184, 177, 38, "确认", confirm_accept, u, NULL);
    lv_obj_set_hidden(u->confirmation, true);
    u->calibration_timer = lv_timer_create(calibration_tick, 100, u); lv_timer_pause(u->calibration_timer);
    p->application_context = u; p->navigation_changed = navigation_changed; p->result_completed = result_completed;
    instrument_ui_apply(u); message(u, startup ? startup : ""); render(u);
}
