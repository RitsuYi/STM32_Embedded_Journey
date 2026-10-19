#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "ui_draw.h"
#include "../fonts/measurement_fonts.h"

/* 私用Unicode编号对应随工程保存的Font Awesome完整字形。
 * 文本字体与图标字体分别指定，避免中文字体缺少图标时显示空框或残缺轮廓。 */
#define UI_ICON_SETTINGS "\uF013"
#define UI_ICON_MEASURE  "\uF545"
#define UI_ICON_INSPECT  "\uF46C"
#define UI_ICON_STATS    "\uF080"
#define UI_ICON_RECORDS  "\uF15C"
#define UI_ICON_WIFI     "\uF1EB"
#define UI_ICON_WIFI_OFF "\uE001"
#define UI_ICON_ZOOM_IN  "\uF00E"
#define UI_ICON_ZOOM_OUT "\uF010"
#define UI_ICON_FIT      "\uF065"
#define UI_ICON_PASS     "\uF058"
#define UI_ICON_FAIL     "\uF057"
#define UI_ICON_TARGET   "\uF140"
#define UI_ICON_PLAY     "\uF04B"
#define UI_ICON_STOP     "\uF04D"
#define UI_ICON_NEXT     "\uF054"
#define UI_ICON_CAMERA   "\uF030"

/* 页脚的原创轻轮廓使用独立字体，不与上面的实心Font Awesome编号混用。 */
#define UI_FOOTER_INSPECT  "\uE101"
#define UI_FOOTER_MEASURE  "\uE102"
#define UI_FOOTER_SETTINGS "\uE103"
#define UI_FOOTER_PLAY     "\uE104"
#define UI_FOOTER_STOP     "\uE105"

/* x/y是图标栏左上角。16px标题用24像素栏，20px按钮用28像素栏。
 * 使用字体本身的行高，不把绘图框写成固定16px，避免上沿/下沿被裁掉。 */
static inline void ui_icon(lv_layer_t *layer,int x,int y,unsigned size,uint32_t color,const char *symbol)
{
    ui_text(layer,x,y,size==20?28:24,size==20?&ui_icons_20:&ui_icons_16,color,symbol,LV_TEXT_ALIGN_CENTER);
}

#endif
