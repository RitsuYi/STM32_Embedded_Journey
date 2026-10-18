#ifndef MEASUREMENT_UI_DRAW_H
#define MEASUREMENT_UI_DRAW_H

#include "lvgl/lvgl.h"

/* 浅蓝白色仪器主题，RGB565会量化到屏幕实际能显示的颜色。 */
/* 取RGB565转换后仍偏浅蓝的一档，避免红/蓝已满而绿色少一档时底色泛粉。 */
#define UI_BG       0xF3FAFF
#define UI_PANEL    0xFFFFFF
#define UI_PLOT     0xC5D9EB
#define UI_GRID     0xB3C9DB
#define UI_BORDER   0xDCE8F6
#define UI_WHITE    0xFFFFFF
#define UI_INK      0x60768E
#define UI_MUTED    0x7D94AC
#define UI_UNIT     0x6B80A2
#define UI_CYAN     0x7DBEFF
#define UI_WIFI     0x69AFFF
#define UI_YELLOW   0xFFFF62
#define UI_TAG      0x809AB4
#define UI_SELECTED 0x217AFF
#define UI_GREEN    0x8ADCB0
#define UI_PURPLE   0xBAA6F5

/* LVGL内部先用完整的8位颜色做透明混合，最后才转RGB565。
 * 不在混合前把白色预先压成248/252/248，否则半透明文字、高光会再次损失亮度。
 * 深色文字采用蓝灰色，避免本屏上接近纯黑的笔画显得过重；纯白仍能输出0xFFFF。 */
static inline lv_color_t ui_color(uint32_t rgb)
{
    return lv_color_hex(rgb);
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
    /* 绘图回调中的临时字符串离开函数后失效，交给LVGL复制再异步绘制。 */
    d.text_local = 1;
    lv_area_t a = {x, y, x + width - 1, y + font->line_height - 1};
    lv_draw_label(layer, &d, &a);
}

/* 半透明网格只轻微提亮背景，不能像首版那样盖住材质和主体。 */
static inline void ui_line_opa(lv_layer_t *layer,int x1,int y1,int x2,int y2,uint32_t color,int width,lv_opa_t opa)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){x1,y1};d.p2=(lv_point_precise_t){x2,y2};
    d.color=ui_color(color);d.width=width;d.opa=opa;lv_draw_line(layer,&d);
}

/* 直接绘制渐变卡片，不分配一整张图片或额外帧缓冲，适合F407的内存。 */
static inline void ui_gradient(lv_layer_t *layer, int x, int y, int w, int h,
                               uint32_t top, uint32_t bottom, int radius, uint32_t border)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = ui_color(top); d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = LV_GRAD_DIR_VER; d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0].color = ui_color(top); d.bg_grad.stops[0].opa = LV_OPA_COVER;
    d.bg_grad.stops[0].frac = 0;
    d.bg_grad.stops[1].color = ui_color(bottom); d.bg_grad.stops[1].opa = LV_OPA_COVER;
    d.bg_grad.stops[1].frac = 255;
    d.radius = radius; d.border_width = border ? 1 : 0;
    d.border_color = ui_color(border);
    lv_area_t a = {x,y,x+w-1,y+h-1};
    lv_draw_rect(layer, &d, &a);
}

/* 圆弧用于外径/内径图标和合格率环；角度从右侧0度开始。 */
static inline void ui_arc(lv_layer_t *layer, int x, int y, int radius, int width,
                          uint32_t color, int start, int end)
{
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = (lv_point_t){x,y}; d.radius = (uint16_t)radius; d.width = width;
    d.color = ui_color(color); d.start_angle = start; d.end_angle = end;
    d.rounded = 1; lv_draw_arc(layer, &d);
}

#endif
