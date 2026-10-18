#ifndef UI_ASSETS_H
#define UI_ASSETS_H
#include "lvgl/lvgl.h"
/* RGB565+A8：彩色像素和透明度分开存放，常驻Flash，绘制时不分配整屏缓存。 */
LV_IMAGE_DECLARE(ui_metal_ring);
LV_IMAGE_DECLARE(ui_plot_background);
LV_IMAGE_DECLARE(ui_metal_shadow);
#endif
