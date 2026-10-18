#ifndef GEOMETRY_VIEW_H
#define GEOMETRY_VIEW_H

#include "lvgl/lvgl.h"
#include "measurement.h"

#define GEOMETRY_DEFAULT_SCALE_Q8 1450U /* 约5.66像素/mm；大工件仍自动适配预览区。 */

typedef struct {
    int32_t cx, cy;
    int32_t x1, y1, x2, y2;
    int32_t inner_radius;
    int32_t dimension_y, dimension_x;
    uint32_t effective_scale_q8;
} GeometryLayout;

typedef struct {
    lv_obj_t * object;
    MeasurementResult result;
    uint32_t scale_q8;
    GeometryLayout layout;
    bool has_geometry;
    bool show_grid, show_dimensions, show_contour, inches;
    bool quality_mode; /* 质检使用绿色尺寸线，普通测量使用黄色。 */
    bool automatic; /* 停止后不继续显示“检测中”。 */
    char primary_text[24];
    char secondary_text[24];
} GeometryView;

/* Returned coordinates are local to the 300 x 229 plot. Scale is isotropic.
 * Oversized parts are fitted inside the plot including annotation clearance. */
bool geometry_layout_calculate(const MeasurementResult * result, uint32_t scale_q8,
                               GeometryLayout * layout);
void geometry_view_create(GeometryView * view, lv_obj_t * parent);
bool geometry_view_update(GeometryView * view, const MeasurementResult * result);
void geometry_view_clear(GeometryView * view);
bool geometry_view_set_scale(GeometryView * view, uint32_t scale_q8);
void geometry_view_set_options(GeometryView * view, bool grid, bool dimensions, bool contour, bool inches);

#endif
