#ifndef MEASUREMENT_UI_DRAW_H
#define MEASUREMENT_UI_DRAW_H

#include "lvgl/lvgl.h"
#include "../assets/ui_skin.h"

/* 保留已认可的8位设计配色；静态敏感渐变预计算抖色，其他图元由LVGL直接混合到565。
 * 背景只轻微减少蓝分量，保留仪器界面的洁净感，不把白色面板染黄。 */
#define UI_BG       0xFAF9F6
#define UI_PANEL    0xFFFFFF
#define UI_PLOT     0xC5D9EB
#define UI_GRID     0xB3C9DB
#define UI_BORDER   0xE0E5EA
#define UI_WHITE    0xFFFFFF
#define UI_INK      0x78899B
#define UI_MUTED    0x8E9DAC
#define UI_UNIT     0x8795A8
#define UI_CYAN     0x98CBFA
#define UI_WIFI     0x87BCF5
#define UI_YELLOW   0xFFFF62
#define UI_TAG      0x809AB4
#define UI_SELECTED 0x98CBFA
#define UI_GREEN    0x8ADCB0
#define UI_PURPLE   0xBAA6F5
/* 尺寸线覆盖在浅色背景和金属工件上，单独使用深蓝灰，保证线与数字都清楚。 */
#define UI_DIMENSION 0x49627A

/* 保留原始8位颜色输入，由LVGL在目标565上进行透明混合。
 * 不在混合前把白色预先压成248/252/248，否则半透明文字、高光会再次损失亮度。
 * 深色文字采用蓝灰色，避免本屏上接近纯黑的笔画显得过重；纯白仍能输出0xFFFF。 */
static inline lv_color_t ui_color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

/* 侧滑菜单是靠屏幕右边的不透明矩形。整条刷新带同时包含两页时，LVGL未必能
 * 自动跳过菜单下面的自绘内容；这里把本次自绘限制到菜单左侧真正露出的区域。
 * saved保存原裁剪框，调用者结束后必须恢复，避免影响后面绘制的菜单/兄弟对象。
 * 只有菜单完全遮住当前裁剪框的高度且没有透明/圆角时才采用此优化，其他情况原样画。
 * 返回false表示这次自绘完全被盖住，无须创建文字、缩放图片等绘图任务。 */
static inline bool ui_underlay_begin(lv_layer_t *layer,lv_obj_t *cover,lv_area_t *saved)
{
    *saved=layer->_clip_area;
    if(!cover || lv_obj_is_hidden(cover) ||
       lv_obj_get_style_bg_opa(cover,LV_PART_MAIN)!=LV_OPA_COVER ||
       lv_obj_get_style_opa(cover,LV_PART_MAIN)!=LV_OPA_COVER ||
       lv_obj_get_style_radius(cover,LV_PART_MAIN)!=0) return true;
    lv_area_t a;lv_obj_get_coords(cover,&a);
    if(a.y1>saved->y1 || a.y2<saved->y2 || a.x2<saved->x2 || a.x1>saved->x2) return true;
    if(a.x1<=saved->x1) return false;
    layer->_clip_area.x2=a.x1-1;
    return true;
}

/* 先看图元包围盒是否碰到当前条带，避免LVGL为最终被裁掉的图元分配绘图任务。
 * margin保留线宽和抗锯齿边缘；只筛任务，不改变原图元坐标，防止条带端点接缝。 */
static inline bool ui_visible(lv_layer_t *layer,int x,int y,int w,int h,int margin)
{
    const lv_area_t *clip=&layer->_clip_area;
    return x-margin<=clip->x2 && x+w-1+margin>=clip->x1 &&
           y-margin<=clip->y2 && y+h-1+margin>=clip->y1;
}

/* Descriptors live on the stack; LVGL copies them into its transient draw tasks.
 * Text is owned by the page/view and remains valid until the draw completes. */
static inline void ui_rect(lv_layer_t * layer, int32_t x, int32_t y, int32_t w, int32_t h,
                           uint32_t color, lv_opa_t opa, int32_t radius,
                           uint32_t border, int32_t border_width)
{
    if(!ui_visible(layer,x,y,w,h,2)) return;
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
    if(!ui_visible(layer,LV_MIN(x1,x2),LV_MIN(y1,y2),LV_ABS(x2-x1)+1,LV_ABS(y2-y1)+1,width+2)) return;
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
    if(!ui_visible(layer,x,y,width,font->line_height,2)) return;
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

/* 仅用于GUI对象持有的缓存文字：到当前条带绘制结束前不得更改或释放。
 * 不再为每个条带复制同一文字；栈上的snprintf结果仍必须使用上面的ui_text。 */
static inline void ui_text_cached(lv_layer_t *layer,int x,int y,int width,
                                  const lv_font_t *font,uint32_t color,const char *text)
{
    if(!ui_visible(layer,x,y,width,font->line_height,2)) return;
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);
    d.font=font;d.color=ui_color(color);d.text=text;d.align=LV_TEXT_ALIGN_LEFT;
    d.text_local=0;
    lv_area_t a={x,y,x+width-1,y+font->line_height-1};
    lv_draw_label(layer,&d,&a);
}

/* 半透明网格只轻微提亮背景，不能像首版那样盖住材质和主体。 */
static inline void ui_line_opa(lv_layer_t *layer,int x1,int y1,int x2,int y2,uint32_t color,int width,lv_opa_t opa)
{
    if(!ui_visible(layer,LV_MIN(x1,x2),LV_MIN(y1,y2),LV_ABS(x2-x1)+1,LV_ABS(y2-y1)+1,width+2)) return;
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){x1,y1};d.p2=(lv_point_precise_t){x2,y2};
    d.color=ui_color(color);d.width=width;d.opa=opa;lv_draw_line(layer,&d);
}

/* 直接绘制渐变卡片，不分配一整张图片或额外帧缓冲，适合F407的内存。 */
static inline void ui_gradient(lv_layer_t *layer, int x, int y, int w, int h,
                               uint32_t top, uint32_t bottom, int radius, uint32_t border)
{
    if(!ui_visible(layer,x,y,w,h,2)) return;
    /* 静态卡片用Flash中的抖色图案，圆角仍交LVGL抗锯齿，边框保持原样。
     * 未命中表的动态工件渐变继续用原生实现，尺寸/缩放能力不受限制。 */
    if(ui_skin_draw_gradient(layer,x,y,w,h,top,bottom,radius)) {
        if(border) ui_rect(layer,x,y,w,h,0,LV_OPA_TRANSP,radius,border,1);
        return;
    }
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
    if(!ui_visible(layer,x-radius,y-radius,2*radius+1,2*radius+1,width+2)) return;
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = (lv_point_t){x,y}; d.radius = (uint16_t)radius; d.width = width;
    d.color = ui_color(color); d.start_angle = start; d.end_angle = end;
    d.rounded = 1; lv_draw_arc(layer, &d);
}

#endif
