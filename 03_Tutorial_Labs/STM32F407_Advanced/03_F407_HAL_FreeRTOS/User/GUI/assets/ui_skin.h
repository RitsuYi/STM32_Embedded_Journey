#ifndef UI_SKIN_H
#define UI_SKIN_H
#include "lvgl/lvgl.h"
/* 历史索引资源和当前预计算565渐变均逐行解码，不展开整图。 */
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
/* 原尺寸静态雾蓝渐变，离线8位计算/有序抖色，运行时只解码相交行。 */
void ui_skin_draw_plot(lv_layer_t *layer,int x,int y,bool quality_mode);
/* 固定卡片命中预计算表时绘制并返回true；其他尺寸返回false，由调用者保留LVGL渐变。 */
bool ui_skin_draw_gradient(lv_layer_t *layer,int x,int y,int w,int h,
                            uint32_t top,uint32_t bottom,int radius);
/* 按尺寸/配色取得常驻Flash描述符；用于原生按钮的背景图片，未命中返回NULL。 */
const lv_image_dsc_t *ui_skin_gradient_source(int w,int h,uint32_t top,uint32_t bottom);
/* 唯一几何视图的可缩放矩形材质；每行只算8个抖色相位，保留任意尺寸。 */
void ui_skin_draw_material(lv_layer_t *layer,int x,int y,int w,int h);
#endif
