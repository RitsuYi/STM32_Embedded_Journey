#ifndef MEASUREMENT_UI_DRAW_H
#define MEASUREMENT_UI_DRAW_H

#include "lvgl/lvgl.h"

#define UI_BG       0x121922
#define UI_PANEL    0x253041
#define UI_PLOT     0x1A2330
#define UI_GRID     0x2C3645
#define UI_BORDER   0x374557
#define UI_WHITE    0xF5F7FC
#define UI_MUTED    0xADBFD0
#define UI_UNIT     0x96B0C5
#define UI_CYAN     0x79D5F0
#define UI_YELLOW   0xF1CC68
#define UI_TAG      0x323B45
#define UI_SELECTED 0x40708B
#define UI_GREEN    0x356B5B

/* Choose the nearest displayed RGB565 level rather than truncating a channel.
 * SDL expands 5/6-bit channels with integer scaling, as a 16-bit panel does. */
static inline uint8_t ui_channel_565(uint8_t value, unsigned bits)
{
    unsigned max = (1U << bits) - 1U;
    unsigned lo = (unsigned)value * max / 255U;
    unsigned hi = LV_MIN(max, lo + 1U);
    unsigned level = (hi * 255U / max - value < value - lo * 255U / max) ? hi : lo;
    return (uint8_t)(level << (8U - bits));
}

static inline lv_color_t ui_color(uint32_t rgb)
{
    return lv_color_make(ui_channel_565((uint8_t)(rgb >> 16), 5),
                         ui_channel_565((uint8_t)(rgb >> 8), 6), ui_channel_565((uint8_t)rgb, 5));
}

/* Descriptors live on the stack; LVGL copies them into its transient draw tasks.
 * Text is owned by the page/view and remains valid until the draw completes. */
static inline void ui_rect(lv_layer_t * layer, int32_t x, int32_t y, int32_t w, int32_t h,
                           uint32_t color, lv_opa_t opa, int32_t radius,
                           uint32_t border, int32_t border_width)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = opa == LV_OPA_COVER ? ui_color(color) : lv_color_hex(color);
    d.bg_opa = opa;
    d.radius = radius;
    d.border_color = ui_color(border);
    d.border_width = border_width;
    d.border_opa = border_width ? LV_OPA_COVER : LV_OPA_TRANSP;
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &d, &a);
}

static inline void ui_line(lv_layer_t * layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                           uint32_t color, int32_t width)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
    d.color = ui_color(color);
    d.width = width;
    lv_draw_line(layer, &d);
}

static inline void ui_text(lv_layer_t * layer, int32_t x, int32_t y, int32_t width,
                           const lv_font_t * font, uint32_t color, const char * text,
                           lv_text_align_t align)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = font;
    d.color = ui_color(color);
    d.text = text;
    d.align = align;
    d.text_local = 0;
    lv_area_t a = {x, y, x + width - 1, y + font->line_height - 1};
    lv_draw_label(layer, &d, &a);
}

#endif
