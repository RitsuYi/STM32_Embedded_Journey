#ifndef UI_ASSETS_H
#define UI_ASSETS_H
#include "lvgl/lvgl.h"
/* 圆环ARGB8888保留8位颜色和透明度；柔影A8，历史背景RGB565。
 * 全部常驻Flash，绘制时不分配整屏缓存。 */
LV_IMAGE_DECLARE(ui_metal_ring);
LV_IMAGE_DECLARE(ui_plot_background);
LV_IMAGE_DECLARE(ui_metal_shadow);
#endif
