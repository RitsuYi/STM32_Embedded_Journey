#include "geometry_view.h"
#include "ui_draw.h"
#include "../fonts/measurement_fonts.h"
#include <stdio.h>
#include "../assets/ui_assets.h"
#include "../assets/ui_skin.h"

static int32_t to_px(int32_t cmm, uint32_t scale)
{
    return (int32_t)(((int64_t)cmm * scale + 12800) / 25600);
}

bool geometry_layout_calculate(const MeasurementResult * r, uint32_t scale, GeometryLayout * p)
{
    if(!p || !measurement_result_check(r) || scale < 128 || scale > 4096) return false;
    /* Clearance reserves room for the width label and the left-bottom axes. */
    int32_t limit_w = 160;
    int32_t limit_h = 160;
    if(r->geometry == MEASUREMENT_RECTANGLE) {
        char value[20];
        lv_point_t text_size;
        measurement_format_cmm(value, sizeof(value), (uint32_t)r->secondary_cmm);
        lv_text_get_size(&text_size, value, &ui_font_14_bold, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        limit_w = 138;
        limit_h = 120;
    }
    int32_t height_cmm = r->geometry == MEASUREMENT_RING ? r->primary_cmm : r->secondary_cmm;
    uint32_t fit_w = (uint32_t)(((int64_t)limit_w * 25600) / r->primary_cmm);
    uint32_t fit_h = (uint32_t)(((int64_t)limit_h * 25600) / height_cmm);
    if(scale > fit_w) scale = fit_w;
    if(scale > fit_h) scale = fit_h;
    /* Q8 cannot represent an arbitrarily small scale. Reject instead of overflow. */
    if(!scale) return false;
    int32_t w = to_px(r->primary_cmm, scale);
    int32_t h = to_px(height_cmm, scale);
    if(w < 4 || h < 4) return false;
    p->cx = r->geometry == MEASUREMENT_RING ? 146 : 115;
    p->cy = r->geometry == MEASUREMENT_RING ? 128 : 128;
    p->x1 = p->cx - w / 2; p->x2 = p->x1 + w;
    p->y1 = p->cy - h / 2; p->y2 = p->y1 + h;
    /* 金属图片是示意工件，孔比例按参考图材质呈现；尺寸数字来自测量结果。
     * 后续接真实轮廓时用相机轮廓替换此示意资产，不把示意图当成图像测量数据。 */
    p->inner_radius = r->geometry == MEASUREMENT_RING ? w * 27 / 100 : 0;
    if(r->geometry == MEASUREMENT_RING &&
       (p->inner_radius < 2 || p->inner_radius >= w / 2 - 1)) return false;
    p->dimension_y = p->y1 - (r->geometry == MEASUREMENT_RING ? 6 : 21);
    p->dimension_x = p->x2 + 13;
    p->effective_scale_q8 = scale;
    return true;
}

/* 网格和标注仍由LVGL绘制，二维工件贴图只作示意；真实轮廓以后从相机接入。 */
static void arrow(lv_layer_t *l,int x,int y,int dx,int dy,uint32_t color)
{
    lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.color=ui_color(color);
    d.p[0]=(lv_point_precise_t){x,y};
    d.p[1]=(lv_point_precise_t){x+dx*5-dy*3,y+dy*5+dx*3};
    d.p[2]=(lv_point_precise_t){x+dx*5+dy*3,y+dy*5-dx*3};lv_draw_triangle(l,&d);
}
/* 位图资源直接从Flash读入LVGL的局部绘图流程；变换只分配小条带，
 * 没有创建整屏RGBA缓存，继续沿用已验收的480×20 RGB565刷新路径。 */
static void image_xy(lv_layer_t *l,const lv_image_dsc_t *src,int x,int y,int sx,int sy)
{
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=src;
    d.scale_x=sx;d.scale_y=sy;d.pivot=(lv_point_t){0,0};d.antialias=1;
    d.recolor=ui_color(0x8CA9BD); /* A8柔影改浅蓝灰，避免暗影盖黑预览背景。 */
    lv_area_t a={x,y,x+(int)src->header.w-1,y+(int)src->header.h-1};lv_draw_image(l,&d,&a);
}
static void image_at(lv_layer_t *l,const lv_image_dsc_t *src,int x,int y,int scale)
{image_xy(l,src,x,y,scale,scale);}
static void draw(lv_event_t *e)
{
    GeometryView *v=lv_event_get_user_data(e);lv_layer_t *l=lv_event_get_layer(e);
    lv_area_t a;lv_obj_get_coords(v->object,&a);int ox=a.x1,oy=a.y1;
    int width=lv_area_get_width(&a);
    /* 在屏幕最终分辨率生成RGB565，避免146×116贴图放大后把色阶放成方块。 */
    ui_skin_draw_plot(l,ox,oy,v->quality_mode);
    /* 细网格与背景只保留很小亮度差；避免首版过亮、过密的棋盘效果。 */
    if(v->show_grid) {
        /* 去掉10像素密网格，只保留参考图的主虚线，避免与色阶叠成棋盘。 */
        const int mx[]={27,v->quality_mode?138:146,width-27},my[]={84,128,174};
        for(unsigned i=0;i<3;++i) {
            for(int y=3;y<230;y+=9)ui_line_opa(l,ox+mx[i],oy+y,ox+mx[i],oy+y+4,0xEDF5FF,1,110);
            for(int x=3;x<width-2;x+=9)ui_line_opa(l,ox+x,oy+my[i],ox+x+4,oy+my[i],0xEDF5FF,1,110);
        }
    }
    ui_rect(l,ox,oy,width,232,0,0,9,UI_WHITE,1);
    ui_line(l,ox+11,oy+217,ox+11,oy+200,0x17CA78,2);arrow(l,ox+11,oy+199,0,1,0x17CA78);
    ui_line(l,ox+11,oy+217,ox+28,oy+217,0xFF4F75,2);arrow(l,ox+29,oy+217,-1,0,0xFF4F75);
    ui_text(l,ox+7,oy+185,12,&ui_font_12,0x16B76A,"Y",LV_TEXT_ALIGN_LEFT);
    ui_text(l,ox+31,oy+211,14,&ui_font_12,0xF13F62,"X",LV_TEXT_ALIGN_LEFT);
    char scale_text[28];uint32_t q=v->has_geometry?v->layout.effective_scale_q8:v->scale_q8;
    snprintf(scale_text,sizeof(scale_text),"%.1fmm/div",2560.0/q);
    ui_rect(l,ox+width-76,oy+209,70,20,0x7892AB,170,6,0xD7E5F2,1);
    ui_text(l,ox+width-72,oy+211,65,&ui_font_12,UI_WHITE,scale_text,LV_TEXT_ALIGN_LEFT);
    /* 普通页没有参考图之外的“工件测量预览”标题条；质检才显示自动状态。 */
    if(v->quality_mode) {
        ui_rect(l,ox+5,oy+5,87,20,0x5B96C8,230,9,0,0);
        ui_arc(l,ox+16,oy+13,5,2,UI_WHITE,0,360);
        ui_line(l,ox+16,oy+6,ox+16,oy+20,0x397EB8,1);ui_line(l,ox+9,oy+13,ox+23,oy+13,0x397EB8,1);
        ui_text(l,ox+25,oy+7,66,&ui_font_12,UI_WHITE,v->automatic?"自动检测中":"质检已停止",LV_TEXT_ALIGN_LEFT);
        ui_text(l,ox+8,oy+28,185,&ui_font_12,UI_WHITE,v->automatic?"已识别工件，正在检测...":"点击启动，等待检测",LV_TEXT_ALIGN_LEFT);
    }
    if(!v->has_geometry||!v->show_contour) {
        ui_text(l,ox+40,oy+107,212,&ui_font_14_bold,UI_WHITE,
            v->show_contour?"放置工件后开始测量":"轮廓已隐藏",LV_TEXT_ALIGN_CENTER);return;
    }
    GeometryLayout p=v->layout;
    if(v->quality_mode) {
        /* 质检右栏更宽，示意工件按参考图向左移并略缩小；测量值本身不变。 */
        /* 顶部状态文字放大后留出16像素，避免与外径尺寸线/数字重叠。 */
        int w=p.x2-p.x1,h=p.y2-p.y1;p.cx-=8;p.cy+=16;
        w=w*96/100;h=h*96/100;p.x1=p.cx-w/2;p.x2=p.x1+w;
        p.y1=p.cy-h/2;p.y2=p.y1+h;p.inner_radius=p.inner_radius*96/100;
        p.dimension_y=p.y1-(v->result.geometry==MEASUREMENT_RING?6:21);p.dimension_x=p.x2+13;
    }
    p.cx+=ox;p.cy+=oy;p.x1+=ox;p.x2+=ox;
    p.y1+=oy;p.y2+=oy;p.dimension_y+=oy;p.dimension_x+=ox;
    bool ring=v->result.geometry==MEASUREMENT_RING;int w=p.x2-p.x1,h=p.y2-p.y1;
    if(ring) {
        /* 参考图式柔影是静态透明贴图，无三维模型或实时光照。 */
        int shade_scale=(w+32)*256/96,shade_size=96*shade_scale/256;
        image_at(l,&ui_metal_shadow,p.cx-shade_size/2+3,p.cy-shade_size/2+6,shade_scale);
        /* 192px源图中的外圆约168px，据真实显示直径等比缩放，保留细金属纹理与内孔阴影。 */
        int scale=w*256/168;int full=(192*scale+255)/256;
        image_at(l,&ui_metal_ring,p.cx-full/2,p.cy-full/2,scale);
        /* 以半透明白色圆环轻提亮材质，保持内孔和外部透明。
         * 高光与材质分别控制，保留既有位图的缩放与透明度处理路径。 */
        lv_draw_arc_dsc_t highlight;lv_draw_arc_dsc_init(&highlight);
        highlight.center=(lv_point_t){p.cx,p.cy};highlight.radius=(uint16_t)(w/2);
        highlight.width=w/2-p.inner_radius;highlight.color=ui_color(UI_WHITE);highlight.opa=36;
        highlight.start_angle=0;highlight.end_angle=360;lv_draw_arc(l,&highlight);
    } else {
        ui_rect(l,p.x1+2,p.y1+4,w+1,h+1,0x536B84,80,4,0,0);
        ui_gradient(l,p.x1,p.y1,w+1,h+1,0xEFF8FF,0xAEC7DA,4,0xFFFFFF);
        for(int y=p.y1+3;y<p.y2-2;y+=3)for(int x=p.x1+3;x<p.x2-2;x+=4)
            if(((x*37+y*19)%11)<3)ui_line(l,x,y,x+1,y,0xD0E4F0,1);
    }
    /* 中心参考线只在工件中心细虚线标记，避免把整圈涂成醒目白线。 */
    for(int y=oy+3;y<oy+230;y+=9)ui_line(l,p.cx,y,p.cx,y+4,0xD7E6F0,1);
    ui_line(l,p.cx-6,p.cy,p.cx+6,p.cy,UI_WHITE,1);ui_line(l,p.cx,p.cy-6,p.cx,p.cy+6,UI_WHITE,1);
    if(!v->show_dimensions)return;
    uint32_t c=v->quality_mode?0xAEFF76:0xFFFF63;
    ui_line(l,p.x1,p.dimension_y-4,p.x1,ring?p.cy:p.y1,c,1);
    ui_line(l,p.x2,p.dimension_y-4,p.x2,ring?p.cy:p.y1,c,1);
    ui_line(l,p.x1,p.dimension_y,p.x2,p.dimension_y,c,1);
    arrow(l,p.x1,p.dimension_y,1,0,c);arrow(l,p.x2,p.dimension_y,-1,0,c);
    ui_text(l,p.cx-55,p.dimension_y-15,110,&ui_font_14_bold,c,v->primary_text,LV_TEXT_ALIGN_CENTER);
    if(ring) {
        int left=p.cx-p.inner_radius,right=p.cx+p.inner_radius;
        ui_line(l,left,p.cy-9,left,p.cy+5,c,1);ui_line(l,right,p.cy-9,right,p.cy+5,c,1);
        ui_line(l,left,p.cy-7,right,p.cy-7,c,1);
        arrow(l,left,p.cy-7,1,0,c);arrow(l,right,p.cy-7,-1,0,c);
        ui_text(l,p.cx-50,p.cy-24,100,&ui_font_14_bold,c,v->secondary_text,LV_TEXT_ALIGN_CENTER);
    } else {
        ui_line(l,p.x2,p.y1,p.dimension_x+4,p.y1,c,1);ui_line(l,p.x2,p.y2,p.dimension_x+4,p.y2,c,1);
        ui_line(l,p.dimension_x,p.y1,p.dimension_x,p.y2,c,1);
        arrow(l,p.dimension_x,p.y1,0,1,c);arrow(l,p.dimension_x,p.y2,0,-1,c);
        ui_text(l,p.dimension_x+7,p.cy-8,69,&ui_font_14_bold,c,v->secondary_text,LV_TEXT_ALIGN_LEFT);
    }
}

void geometry_view_create(GeometryView * v, lv_obj_t * parent)
{
    *v = (GeometryView){0};
    v->object = lv_obj_create(parent);
    lv_obj_remove_style_all(v->object);
    lv_obj_set_pos(v->object, 8, 36);
    lv_obj_set_size(v->object, 292, 232);
    lv_obj_set_style_bg_color(v->object, ui_color(UI_PLOT), 0);
    lv_obj_set_style_bg_opa(v->object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(v->object, ui_color(UI_BORDER), 0);
    lv_obj_set_style_border_width(v->object, 1, 0);
    lv_obj_set_style_radius(v->object, 5, 0);
    lv_obj_set_scrollable(v->object, false);
    lv_obj_set_clickable(v->object, false);
    v->scale_q8 = GEOMETRY_DEFAULT_SCALE_Q8;
    v->show_grid = v->show_dimensions = v->show_contour = true;
    lv_obj_add_event_cb(v->object, draw, LV_EVENT_DRAW_MAIN, v);
}

bool geometry_view_update(GeometryView * v, const MeasurementResult * r)
{
    GeometryLayout layout;
    if(!v || !v->object || !measurement_result_check(r)) return false;
    /* Status, connection and residual changes do not affect the plot. */
    if(v->has_geometry && v->result.geometry == r->geometry &&
       v->result.primary_cmm == r->primary_cmm && v->result.secondary_cmm == r->secondary_cmm) {
        v->result = *r;
        return true;
    }
    if(!geometry_layout_calculate(r, v->scale_q8, &layout)) return false;
    v->result = *r; v->layout = layout;
    v->has_geometry = true;
    char number[20];
    if(v->inches) snprintf(number, sizeof(number), "%.3f", r->primary_cmm / 2540.0);
    else measurement_format_cmm(number, sizeof(number), (uint32_t)r->primary_cmm);
    snprintf(v->primary_text, sizeof(v->primary_text), "%s%s", r->geometry == MEASUREMENT_RING ? "Ø" : "", number);
    if(v->inches) snprintf(number, sizeof(number), "%.3f", r->secondary_cmm / 2540.0);
    else measurement_format_cmm(number, sizeof(number), (uint32_t)r->secondary_cmm);
    snprintf(v->secondary_text, sizeof(v->secondary_text), "%s%s", r->geometry == MEASUREMENT_RING ? "Ø" : "", number);
    /* Invalidate the entire plot: both the old contour and annotations disappear. */
    lv_obj_invalidate(v->object);
    return true;
}

void geometry_view_clear(GeometryView * v)
{
    if(!v || !v->object || !v->has_geometry) return;
    v->has_geometry = false;
    lv_obj_invalidate(v->object);
}

bool geometry_view_set_scale(GeometryView * v, uint32_t scale)
{
    GeometryLayout layout;
    if(!v || !v->object || scale < 128 || scale > 4096) return false;
    if(v->scale_q8 == scale) return true;
    if(!v->has_geometry) { v->scale_q8 = scale; return true; }
    if(!geometry_layout_calculate(&v->result, scale, &layout)) return false;
    v->scale_q8 = scale;
    /* A changed requested scale can still produce the same fitted layout. */
    bool changed = layout.x1 != v->layout.x1 || layout.x2 != v->layout.x2 ||
                   layout.y1 != v->layout.y1 || layout.y2 != v->layout.y2 ||
                   layout.inner_radius != v->layout.inner_radius;
    v->layout = layout;
    if(changed) lv_obj_invalidate(v->object);
    return true;
}

void geometry_view_set_options(GeometryView * v, bool grid, bool dimensions, bool contour, bool inches)
{
    if(!v || !v->object) return;
    bool units_changed = v->inches != inches;
    v->show_grid = grid; v->show_dimensions = dimensions; v->show_contour = contour; v->inches = inches;
    if(units_changed && v->has_geometry) {
        MeasurementResult result = v->result;
        v->has_geometry = false;
        geometry_view_update(v, &result);
    }
    lv_obj_invalidate(v->object);
}
