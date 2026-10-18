#ifndef UI_SKIN_H
#define UI_SKIN_H
#include "lvgl/lvgl.h"
/* 参考图皮肤采用256色索引，按行还原为RGB565；不会在RAM展开整张图片。 */
LV_IMAGE_DECLARE(skin_header);
LV_IMAGE_DECLARE(skin_measurement);
LV_IMAGE_DECLARE(skin_inspection);
LV_IMAGE_DECLARE(skin_pass);
LV_IMAGE_DECLARE(skin_footer);
LV_IMAGE_DECLARE(skin_button_white);
LV_IMAGE_DECLARE(skin_button_blue);
LV_IMAGE_DECLARE(skin_zoom_in);
LV_IMAGE_DECLARE(skin_zoom_out);
LV_IMAGE_DECLARE(skin_zoom_fit);
LV_IMAGE_DECLARE(skin_settings_icon);
LV_IMAGE_DECLARE(skin_play_icon);
LV_IMAGE_DECLARE(skin_ruler_icon);
void ui_skin_init(void);
void ui_skin_draw(lv_layer_t *layer,const lv_image_dsc_t *source,int x,int y);
/* 原尺寸逐行生成雾蓝渐变；宽度只允许主界面使用的292或278像素。
 * 颜色直接量化成RGB565并做有序抖动，不经过低分辨率放大或256色调色板。 */
void ui_skin_draw_plot(lv_layer_t *layer,int x,int y,bool quality_mode);
#endif
