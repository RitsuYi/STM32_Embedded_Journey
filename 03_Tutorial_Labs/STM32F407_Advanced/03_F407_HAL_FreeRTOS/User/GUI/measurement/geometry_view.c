#include "geometry_view.h"
#include "ui_draw.h"
#include "../fonts/measurement_fonts.h"
#include <stdio.h>

static int32_t to_px(int32_t cmm, uint32_t scale)
{
    return (int32_t)(((int64_t)cmm * scale + 12800) / 25600);
}

bool geometry_layout_calculate(const MeasurementResult * r, uint32_t scale, GeometryLayout * p)
{
    if(!p || !measurement_result_check(r) || scale < 128 || scale > 4096) return false;
    /* Clearance reserves room for the width label and the left-bottom axes. */
    int32_t limit_w = 142;
    int32_t limit_h = 142;
    if(r->geometry == MEASUREMENT_RECTANGLE) {
        char value[20];
        lv_point_t text_size;
        measurement_format_cmm(value, sizeof(value), (uint32_t)r->secondary_cmm);
        lv_text_get_size(&text_size, value, &measurement_font_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        limit_w = LV_MIN(180, 2 * (297 - 143 - 30 - 12 - text_size.x - 3));
        limit_h = 128;
    }
    int32_t height_cmm = r->geometry == MEASUREMENT_RING ? r->primary_cmm : r->secondary_cmm;
    uint32_t fit_w = (uint32_t)(((int64_t)limit_w * 25600) / r->primary_cmm);
    uint32_t fit_h = (uint32_t)(((int64_t)limit_h * 25600) / height_cmm);
    if(scale > fit_w) scale = fit_w;
    if(scale > fit_h) scale = fit_h;
    /* Q8 cannot represent an arbitrarily small scale. Reject instead of overflow. */
    if(!scale) return false;
    int32_t w = to_px(r->primary_cmm, scale);
    int32_t h = to_px(height_cmm, scale);
    if(w < 4 || h < 4) return false;
    p->cx = 143;
    p->cy = r->geometry == MEASUREMENT_RING ? 119 : 114;
    p->x1 = p->cx - w / 2; p->x2 = p->x1 + w;
    p->y1 = p->cy - h / 2; p->y2 = p->y1 + h;
    p->inner_radius = to_px(r->secondary_cmm, scale) / 2;
    if(r->geometry == MEASUREMENT_RING &&
       (p->inner_radius < 2 || p->inner_radius >= w / 2 - 1)) return false;
    p->dimension_y = p->y1 - (r->geometry == MEASUREMENT_RING ? 25 : 29);
    p->dimension_x = p->x2 + 30;
    p->effective_scale_q8 = scale;
    return true;
}

/* Arrow tips point to the two measured endpoints, matching the references. */
static void arrow(lv_layer_t * layer, int32_t x, int32_t y, int32_t dx, int32_t dy)
{
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.color = ui_color(UI_YELLOW);
    d.p[0].x = x; d.p[0].y = y;
    d.p[1].x = x + dx * 7 - dy * 3; d.p[1].y = y + dy * 7 + dx * 3;
    d.p[2].x = x + dx * 7 + dy * 3; d.p[2].y = y + dy * 7 - dx * 3;
    lv_draw_triangle(layer, &d);
}

static void dimension_tag(lv_layer_t * layer, int32_t cx, int32_t y, int32_t min_width,
                          int32_t height, const char * text)
{
    lv_point_t size;
    lv_text_get_size(&size, text, &measurement_font_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int32_t width = LV_MAX(min_width, size.x + 12);
    ui_rect(layer, cx - width / 2, y, width, height, UI_TAG, LV_OPA_COVER, 3, 0, 0);
    ui_text(layer, cx - width / 2, y + (height - measurement_font_14.line_height) / 2,
            width, &measurement_font_14, UI_YELLOW, text, LV_TEXT_ALIGN_CENTER);
}

static void draw(lv_event_t * e)
{
    GeometryView * v = lv_event_get_user_data(e);
    GeometryLayout p = v->layout;
    lv_area_t a;
    lv_obj_get_coords(v->object, &a);
    int32_t ox = a.x1, oy = a.y1;
    lv_layer_t * layer = lv_event_get_layer(e);
    if(v->show_grid) {
        for(int32_t x = 47; x < 298; x += 30) ui_line(layer, ox + x, oy + 2, ox + x, oy + 226, UI_GRID, 1);
        for(int32_t y = 24; y < 227; y += 30) ui_line(layer, ox + 2, oy + y, ox + 297, oy + y, UI_GRID, 1);
    }
    ui_line(layer, ox + 18, oy + 184, ox + 18, oy + 207, 0x80B5CF, 1);
    ui_line(layer, ox + 18, oy + 207, ox + 41, oy + 207, 0x80B5CF, 1);
    ui_text(layer, ox + 12, oy + 170, 12, &measurement_font_12, 0x93BACD, "Y", LV_TEXT_ALIGN_LEFT);
    ui_text(layer, ox + 45, oy + 200, 12, &measurement_font_12, 0x93BACD, "X", LV_TEXT_ALIGN_LEFT);

    if(!v->has_geometry || !v->show_contour) {
        ui_text(layer, ox + 40, oy + 91, 220, &measurement_font_14, UI_MUTED,
                v->show_contour ? "工件预览" : "轮廓已隐藏", LV_TEXT_ALIGN_CENTER);
        ui_text(layer, ox + 30, oy + 118, 240, &measurement_font_12, UI_UNIT,
                v->show_contour ? "完成测量后显示轮廓" : "可在显示设置中开启", LV_TEXT_ALIGN_CENTER);
        return;
    }

    p.cx += ox; p.cy += oy; p.x1 += ox; p.x2 += ox;
    p.y1 += oy; p.y2 += oy; p.dimension_y += oy; p.dimension_x += ox;
    bool ring = v->result.geometry == MEASUREMENT_RING;
    int32_t w = p.x2 - p.x1, h = p.y2 - p.y1;
    /* The alpha-blended fill preserves the grid, without a canvas or extra layer. */
    ui_rect(layer, p.x1, p.y1, w + 1, h + 1, 0x6C7D8C, 128,
            ring ? LV_RADIUS_CIRCLE : 0, UI_CYAN, 2);
    if(ring) {
        int32_t ir = p.inner_radius;
        ui_rect(layer, p.cx - ir, p.cy - ir, ir * 2 + 1, ir * 2 + 1,
                UI_PLOT, LV_OPA_COVER, LV_RADIUS_CIRCLE, UI_CYAN, 2);
    }
    ui_line(layer, p.cx - 5, p.cy, p.cx + 5, p.cy, UI_WHITE, 1);
    ui_line(layer, p.cx, p.cy - 5, p.cx, p.cy + 5, UI_WHITE, 1);
    if(!v->show_dimensions) return;

    int32_t ext_end = ring ? p.cy : p.y1;
    ui_line(layer, p.x1, p.dimension_y - 5, p.x1, ext_end, UI_YELLOW, 1);
    ui_line(layer, p.x2, p.dimension_y - 5, p.x2, ext_end, UI_YELLOW, 1);
    ui_line(layer, p.x1, p.dimension_y, p.x2, p.dimension_y, UI_YELLOW, 1);
    arrow(layer, p.x1, p.dimension_y, 1, 0);
    arrow(layer, p.x2, p.dimension_y, -1, 0);
    dimension_tag(layer, p.cx, p.dimension_y - (ring ? 17 : 18), ring ? 68 : 62, 21, v->primary_text);

    if(ring) {
        int32_t l = p.cx - p.inner_radius, r = p.cx + p.inner_radius;
        int32_t y = p.cy + p.inner_radius + 15;
        ui_line(layer, l, p.cy, l, y + 5, UI_YELLOW, 1);
        ui_line(layer, r, p.cy, r, y + 5, UI_YELLOW, 1);
        ui_line(layer, l, y, r, y, UI_YELLOW, 1);
        arrow(layer, l, y, 1, 0); arrow(layer, r, y, -1, 0);
        dimension_tag(layer, p.cx, y + 6, 58, measurement_font_14.line_height + 2, v->secondary_text);
    }
    else {
        ui_line(layer, p.x2, p.y1, p.dimension_x + 6, p.y1, UI_YELLOW, 1);
        ui_line(layer, p.x2, p.y2, p.dimension_x + 6, p.y2, UI_YELLOW, 1);
        ui_line(layer, p.dimension_x, p.y1, p.dimension_x, p.y2, UI_YELLOW, 1);
        arrow(layer, p.dimension_x, p.y1, 0, 1); arrow(layer, p.dimension_x, p.y2, 0, -1);
        /* Horizontal width text is deliberately outside the part. */
        int32_t label_width = lv_area_get_width(&a) - (p.dimension_x - ox) - 15;
        ui_text(layer, p.dimension_x + 12, p.cy - measurement_font_14.line_height / 2, label_width, &measurement_font_14,
                UI_YELLOW, v->secondary_text, LV_TEXT_ALIGN_LEFT);
    }
}

void geometry_view_create(GeometryView * v, lv_obj_t * parent)
{
    *v = (GeometryView){0};
    v->object = lv_obj_create(parent);
    lv_obj_remove_style_all(v->object);
    lv_obj_set_pos(v->object, 7, 43);
    lv_obj_set_size(v->object, 300, 229);
    lv_obj_set_style_bg_color(v->object, ui_color(UI_PLOT), 0);
    lv_obj_set_style_bg_opa(v->object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(v->object, ui_color(UI_BORDER), 0);
    lv_obj_set_style_border_width(v->object, 1, 0);
    lv_obj_set_style_radius(v->object, 5, 0);
    lv_obj_set_scrollable(v->object, false);
    lv_obj_set_clickable(v->object, false);
    v->scale_q8 = GEOMETRY_DEFAULT_SCALE_Q8;
    v->show_grid = v->show_dimensions = v->show_contour = true;
    lv_obj_add_event_cb(v->object, draw, LV_EVENT_DRAW_MAIN, v);
}

bool geometry_view_update(GeometryView * v, const MeasurementResult * r)
{
    GeometryLayout layout;
    if(!v || !v->object || !measurement_result_check(r)) return false;
    /* Status, connection and residual changes do not affect the plot. */
    if(v->has_geometry && v->result.geometry == r->geometry &&
       v->result.primary_cmm == r->primary_cmm && v->result.secondary_cmm == r->secondary_cmm) {
        v->result = *r;
        return true;
    }
    if(!geometry_layout_calculate(r, v->scale_q8, &layout)) return false;
    v->result = *r; v->layout = layout;
    v->has_geometry = true;
    char number[20];
    if(v->inches) snprintf(number, sizeof(number), "%.3f", r->primary_cmm / 2540.0);
    else measurement_format_cmm(number, sizeof(number), (uint32_t)r->primary_cmm);
    snprintf(v->primary_text, sizeof(v->primary_text), "%s%s", r->geometry == MEASUREMENT_RING ? "Ø" : "", number);
    if(v->inches) snprintf(number, sizeof(number), "%.3f", r->secondary_cmm / 2540.0);
    else measurement_format_cmm(number, sizeof(number), (uint32_t)r->secondary_cmm);
    snprintf(v->secondary_text, sizeof(v->secondary_text), "%s%s", r->geometry == MEASUREMENT_RING ? "Ø" : "", number);
    /* Invalidate the entire plot: both the old contour and annotations disappear. */
    lv_obj_invalidate(v->object);
    return true;
}

void geometry_view_clear(GeometryView * v)
{
    if(!v || !v->object || !v->has_geometry) return;
    v->has_geometry = false;
    lv_obj_invalidate(v->object);
}

bool geometry_view_set_scale(GeometryView * v, uint32_t scale)
{
    GeometryLayout layout;
    if(!v || !v->object || scale < 128 || scale > 4096) return false;
    if(v->scale_q8 == scale) return true;
    if(!v->has_geometry) { v->scale_q8 = scale; return true; }
    if(!geometry_layout_calculate(&v->result, scale, &layout)) return false;
    v->scale_q8 = scale;
    /* A changed requested scale can still produce the same fitted layout. */
    bool changed = layout.x1 != v->layout.x1 || layout.x2 != v->layout.x2 ||
                   layout.y1 != v->layout.y1 || layout.y2 != v->layout.y2 ||
                   layout.inner_radius != v->layout.inner_radius;
    v->layout = layout;
    if(changed) lv_obj_invalidate(v->object);
    return true;
}

void geometry_view_set_options(GeometryView * v, bool grid, bool dimensions, bool contour, bool inches)
{
    if(!v || !v->object) return;
    bool units_changed = v->inches != inches;
    v->show_grid = grid; v->show_dimensions = dimensions; v->show_contour = contour; v->inches = inches;
    if(units_changed && v->has_geometry) {
        MeasurementResult result = v->result;
        v->has_geometry = false;
        geometry_view_update(v, &result);
    }
    lv_obj_invalidate(v->object);
}
