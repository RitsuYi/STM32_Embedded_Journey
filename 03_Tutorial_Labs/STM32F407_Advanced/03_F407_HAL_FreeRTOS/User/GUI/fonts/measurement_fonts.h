#ifndef MEASUREMENT_FONTS_H
#define MEASUREMENT_FONTS_H
#include "lvgl/lvgl.h"
LV_FONT_DECLARE(measurement_font_12);
LV_FONT_DECLARE(measurement_font_14);
LV_FONT_DECLARE(measurement_font_16_bold);
LV_FONT_DECLARE(measurement_font_24_bold);
LV_FONT_DECLARE(measurement_font_28_bold);
/* 新UI只补缺失字形，已有汉字和数字回退到上述原字体，避免重复占用Flash。 */
LV_FONT_DECLARE(dashboard_font_12);
LV_FONT_DECLARE(dashboard_font_14);
LV_FONT_DECLARE(dashboard_font_16_bold);
LV_FONT_DECLARE(dashboard_font_10); /* 质检摘要小表格专用；完整记录页仍使用12/14px。 */
LV_FONT_DECLARE(ui_font_9);
LV_FONT_DECLARE(ui_font_11);
LV_FONT_DECLARE(ui_font_13_bold);
LV_FONT_DECLARE(ui_font_13_numeric);
LV_FONT_DECLARE(ui_font_16_bold);
LV_FONT_DECLARE(ui_font_26_bold);
/* RGB565屏幕使用官方转换器生成的原尺寸字形及独立图标字体。 */
LV_FONT_DECLARE(ui_font_12);
LV_FONT_DECLARE(ui_font_14_bold);
LV_FONT_DECLARE(ui_icons_16);
LV_FONT_DECLARE(ui_icons_20);
#endif
