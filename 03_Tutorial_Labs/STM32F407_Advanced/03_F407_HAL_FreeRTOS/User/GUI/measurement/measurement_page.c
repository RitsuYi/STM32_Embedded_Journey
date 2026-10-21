#include "measurement_page.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "src/core/lv_obj_event_private.h"
#include "../fonts/measurement_fonts.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "../assets/ui_skin.h"

/* 本文件只负责480×320主画面与测量会话。网络、记录和标准由dashboard_ui连接，
 * 所有回调都在GUI任务运行；不额外创建任务，也不改变触摸驱动和SPI刷新时序。 */
static lv_obj_t *plain_object(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_scrollable(o,false); lv_obj_set_clickable(o,false); return o;
}
static void text(lv_layer_t *l,int x,int y,int w,const lv_font_t *f,uint32_t c,const char *s)
{ ui_text(l,x,y,w,f,c,s,LV_TEXT_ALIGN_LEFT); }
/* baseline是文字落在同一条水平线上的位置；不同字号的顶部不能直接共用同一个y。
 * 用字体的行高和基线修正，数字、中文名称和单位就能自然对齐。 */
static void text_baseline(lv_layer_t *l,int x,int baseline,int w,const lv_font_t *f,uint32_t c,const char *s)
{ text(l,x,baseline-f->line_height+f->base_line,w,f,c,s); }
static const char *state_text(const MeasurementSession *s)
{
    switch(s->state) {
    case MEASUREMENT_ACQUIRING:return "采集中";
    case MEASUREMENT_FITTING:return "识别中";
    case MEASUREMENT_COMPLETE:return "测量完成";
    case MEASUREMENT_FAILED:return "测量失败";
    default:return "等待工件";
    }
}
/* 立方体、相机、放大镜和状态图标按参考图绘制，不再用+/-/[]代替。 */
/* 保留参考图卡片布局，以屏幕原尺寸重绘标题、图标、数字和状态。
 * RGB565优化不改变会话、测量值、统计、按键或记录的业务含义。 */
/* 简单图标直接由线、圆和三角形绘制，边缘由LVGL抗锯齿处理。
 * 不再把带矩形底色的裁剪图盖到另一块渐变上，按钮按下时也能自然融合。 */
static void triangle(lv_layer_t *l,int x1,int y1,int x2,int y2,int x3,int y3,uint32_t c)
{
    if(!ui_visible(l,LV_MIN(x1,LV_MIN(x2,x3)),LV_MIN(y1,LV_MIN(y2,y3)),
        LV_MAX(x1,LV_MAX(x2,x3))-LV_MIN(x1,LV_MIN(x2,x3))+1,
        LV_MAX(y1,LV_MAX(y2,y3))-LV_MIN(y1,LV_MIN(y2,y3))+1,2)) return;
    lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.color=ui_color(c);
    d.p[0]=(lv_point_precise_t){x1,y1};d.p[1]=(lv_point_precise_t){x2,y2};
    d.p[2]=(lv_point_precise_t){x3,y3};lv_draw_triangle(l,&d);
}
static void draw_header(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e);lv_layer_t *l=lv_event_get_layer(e);
    ui_gradient(l,0,0,480,30,UI_WHITE,0xFAF9F6,0,0);
    /* 每个菱形面拆成两个三角形，形成完整立方体，白色细缝区分三个面。 */
    triangle(l,21,5,30,10,21,15,0x6CD9FF);triangle(l,21,5,21,15,12,10,0x6CD9FF);
    triangle(l,12,11,20,16,20,25,0x53BFFF);triangle(l,12,11,20,25,12,20,0x53BFFF);
    triangle(l,22,16,30,11,30,20,0x438DF2);triangle(l,22,16,30,20,22,25,0x438DF2);
    text(l,39,6,254,&ui_font_16_bold,UI_INK,"智能视觉测量仪");
    /* 顶部只显示真实UART链路；模拟测量结果不能把相机图标点亮。
     * 图标和文字共用y=21的基线（字形落下的位置），按各自字体行高换算顶部。
     * 文字单独保留64px，足够容纳五个12px汉字，与右侧Wi-Fi图标不重叠。 */
    int icon_y=21-ui_icons_16.line_height+ui_icons_16.base_line;
    /* 相机字形上部较高，基线对齐后再下移2px，让视觉中心贴齐右侧汉字。 */
    ui_icon(l,292,icon_y+2,16,p->camera_online?UI_GREEN:UI_OFFLINE,UI_ICON_CAMERA);
    text_baseline(l,318,21,64,&ui_font_12,UI_MUTED,p->camera_online?"相机已连接":"相机未连接");
    /* Wi-Fi独立显示相机回报的实际联网结果；握手成功并不代表已经连上Wi-Fi。 */
    ui_icon(l,384,icon_y,16,p->wifi_connected?UI_WIFI:UI_OFFLINE,
            p->wifi_connected?UI_ICON_WIFI:UI_ICON_WIFI_OFF);
    ui_line(l,413,7,413,24,0xD5E5F4,1);
    char t[24];uint32_t seconds=p->clock_seconds;
    snprintf(t,sizeof(t),"%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,seconds/3600U,seconds/60U%60U,seconds%60U);
    ui_text(l,419,21-ui_font_12.line_height+ui_font_12.base_line,54,&ui_font_12,UI_MUTED,t,LV_TEXT_ALIGN_RIGHT);
}
static void draw_result_content(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e);lv_layer_t *l=lv_event_get_layer(e);
    /* displayed是已有的最后一次控件同步快照，不额外复制整页或测量模型。
     * 过渡中会话继续变化，底图仍使用同一份可见结果，结束后再统一提交最新值。 */
    const MeasurementSession *s=p->presentation_paused?&p->displayed:&p->session;
    bool ring=s->geometry==MEASUREMENT_RING,complete=s->state==MEASUREMENT_COMPLETE;
    const MeasurementResult *r=&s->result;char value[24],buf[80];
    /* 卡片与文字在屏幕原尺寸绘制；不再保留截图里已经缩小的标题和纯黑笔画。 */
    int left=p->quality_mode?292:307,width=p->quality_mode?183:168;
    ui_gradient(l,left,35,width,237,UI_WHITE,0xFCFBF8,9,UI_BORDER);
    if(!p->quality_mode) {
        ui_icon(l,312,42,16,UI_CYAN,UI_ICON_STATS);
        text(l,336,42,86,&ui_font_14_bold,UI_INK,"本次结果");
        uint32_t seconds=p->presentation_paused?p->presentation_clock:p->clock_seconds;
        snprintf(buf,sizeof(buf),"%02" PRIu32 ":%02" PRIu32,seconds/60U,seconds%60U);
        text(l,430,45,41,&ui_font_12,UI_MUTED,buf);
        for(unsigned i=0;i<2;++i) {
            int y=72+(int)i*47;uint32_t c=i?UI_PURPLE:UI_CYAN;
            ui_gradient(l,312,y-5,158,44,UI_WHITE,0xFBFBF9,7,UI_BORDER);
            ui_arc(l,325,y+16,7,2,c,0,360);
            int32_t n=i?r->secondary_cmm:r->primary_cmm;
            if(complete){if(p->inches)snprintf(value,sizeof(value),"%.3f",n/2540.0);else measurement_format_cmm(value,sizeof(value),(uint32_t)n);}else strcpy(value,"--");
            const lv_font_t *f=strlen(value)>7?&ui_font_12:strlen(value)>5?&ui_font_16_bold:&ui_font_26_bold;
            /* 每行向卡片中部收拢，并共用基线，避免大数字看起来悬在名称上方。 */
            text_baseline(l,338,y+27,31,&ui_font_14_bold,UI_INK,ring?(i?"内径":"外径"):(i?"宽度":"长度"));
            text_baseline(l,373,y+27,74,f,c,value);text_baseline(l,447,y+27,23,&ui_font_12,c,p->inches?"in":"mm");
        }
        ui_icon(l,312,172,16,UI_CYAN,UI_ICON_TARGET);text(l,338,171,125,&ui_font_14_bold,UI_INK,"识别状态");
        text(l,319,194,135,&ui_font_14_bold,UI_MUTED,state_text(s));
        snprintf(buf,sizeof(buf),"%u%%",s->progress);text(l,440,216,33,&ui_font_12,UI_MUTED,buf);
        if(complete)measurement_format_cmm(value,sizeof(value),r->residual_cmm);else strcpy(value,"--");
        text(l,319,238,69,&ui_font_12,UI_MUTED,"拟合残差");text(l,399,238,72,&ui_font_12,UI_MUTED,"测量状态");
        text_baseline(l,319,264,70,&ui_font_16_bold,UI_INK,value);
        text_baseline(l,399,264,72,&ui_font_12,UI_CYAN,state_text(s));
        return;
    }
    /* 四个区域各有独立的完整图标，文字起点统一，不能为调浅颜色而丢掉图标。 */
    ui_icon(l,295,42,16,UI_CYAN,UI_ICON_SETTINGS);
    text(l,321,41,126,&ui_font_14_bold,UI_INK,"标准设定");ui_icon(l,449,43,16,UI_MUTED,UI_ICON_NEXT);
    ui_line(l,298,66,468,66,UI_BORDER,1);
    text(l,321,73,148,&ui_font_14_bold,UI_INK,"当前检测结果");
    ui_gradient(l,296,94,174,39,p->verdict>1?0xFFF3F5:0xF2FCF7,UI_WHITE,7,0);
    /* 超差时用粉红提示覆盖原参考图的合格底色，避免红字仍配绿色结论卡片。 */
    /* 标题图标随本次结果变化，不能把参考图的合格状态作为固定结论。 */
    ui_icon(l,295,74,16,p->verdict==1?UI_GREEN:p->verdict?0xF29AA9:UI_CYAN,
            p->verdict==1?UI_ICON_PASS:p->verdict?UI_ICON_FAIL:UI_ICON_TARGET);
    /* 结论与两行参数各占一列；名称向中间移动，消除结论后方的大块空白。
     * 已完成的质检值保留到下一次有效结果，采集期间不再闪回“--”。 */
    bool quality_result=s->has_result&&p->verdict!=0;
    ui_text(l,300,105,57,&ui_font_16_bold,p->verdict==1?UI_GREEN:p->verdict?0xE97488:UI_MUTED,
            p->verdict==1?"合格":p->verdict?"不合格":"待检测",LV_TEXT_ALIGN_CENTER);
    ui_line(l,359,101,359,126,UI_BORDER,1);
    for(unsigned i=0;i<2;++i){
        int baseline=112+(int)i*19;
        if(quality_result){int32_t n=i?r->secondary_cmm:r->primary_cmm;if(p->inches)snprintf(value,sizeof(value),"%.3f",n/2540.0);else measurement_format_cmm(value,sizeof(value),(uint32_t)n);}else strcpy(value,"--");
        text_baseline(l,392,baseline,54,strlen(value)>6?&ui_font_12:&ui_font_16_bold,i?UI_PURPLE:UI_CYAN,value);
        text_baseline(l,365,baseline,26,&ui_font_12,UI_INK,ring?(i?"内径":"外径"):(i?"宽度":"长度"));
        text_baseline(l,449,baseline,22,&ui_font_12,i?UI_PURPLE:UI_CYAN,p->inches?"in":"mm");
    }
    /* 缩小屏幕下省略括号，保留mm单位，避免表头换行压住第一条记录。 */
    /* 收紧结果卡片下沿与统计留白，把省下的8px交给最近记录，两行间距增至20px。 */
    ui_line(l,298,135,468,135,UI_BORDER,1);ui_icon(l,295,140,16,UI_CYAN,UI_ICON_STATS);
    text(l,321,139,148,&ui_font_14_bold,UI_INK,"累计统计");
    text(l,301,162,39,&ui_font_12,UI_MUTED,"总数");text(l,340,162,39,&ui_font_12,UI_MUTED,"合格");
    text(l,379,162,39,&ui_font_12,UI_MUTED,"超差");
    uint32_t counts[]={p->total_count,p->pass_count,p->fail_count};
    for(unsigned i=0;i<3;++i){snprintf(buf,sizeof(buf),"%" PRIu32,counts[i]);text(l,301+(int)i*39,180,38,strlen(buf)>4?&ui_font_12:&ui_font_16_bold,i==1?UI_GREEN:i==2?0xED778A:UI_INK,buf);}
    uint32_t rate=p->total_count?(uint32_t)((uint64_t)p->pass_count*1000U/p->total_count):0;
    /* 圆环内径40px；整数百分比省略无意义的“.0”，其余仍保留一位小数。
     * 文字按真实字体行高居中，100%也不会伸到圆环笔画上。 */
    ui_arc(l,442,180,24,4,0xE0F1E8,0,360);if(rate)ui_arc(l,442,180,24,4,UI_GREEN,270,270+(int)(rate*360U/1000U));
    if(rate%10U)snprintf(buf,sizeof(buf),"%" PRIu32 ".%" PRIu32 "%%",rate/10,rate%10);
    else snprintf(buf,sizeof(buf),"%" PRIu32 "%%",rate/10);
    ui_text(l,422,180-ui_font_12.line_height/2,40,&ui_font_12,UI_INK,p->total_count?buf:"--",LV_TEXT_ALIGN_CENTER);
    ui_line(l,298,208,468,208,UI_BORDER,1);ui_icon(l,295,212,16,UI_CYAN,UI_ICON_RECORDS);
    text(l,321,211,106,&ui_font_14_bold,UI_INK,"最近检测记录");
    text(l,432,214,35,&ui_font_12,UI_MUTED,"更多 >");
    /* 主界面只保留两条清晰摘要；完整开机时间、单位与结果仍可点箭头在记录页查看。
     * 不把五列内容挤进9像素字体；模型中保存的数据和64条容量没有减少。 */
    for(unsigned i=0;i<2;++i){int y=235+(int)i*20;
        if(!p->recent_records[i].id){if(!i)text(l,299,y,170,&ui_font_12,UI_MUTED,"暂无检测记录");continue;}
        bool pass=p->recent_records[i].pass;
        snprintf(buf,sizeof(buf),"#%" PRIu32,p->recent_records[i].id);text(l,299,y,32,&ui_font_12,UI_MUTED,buf);
        measurement_format_cmm(value,sizeof(value),(uint32_t)p->recent_records[i].primary_cmm);
        measurement_format_cmm(buf,sizeof(buf),(uint32_t)p->recent_records[i].secondary_cmm);
        char pair[52];snprintf(pair,sizeof(pair),"%.16s / %.16s",value,buf);text(l,332,y,105,&ui_font_12,UI_INK,pair);
        text(l,444,y,29,&ui_font_12,pass?UI_GREEN:0xED778A,pass?"合格":"超差");
    }
}
/* 结果卡包含很多文字/圆弧；全被盖住时连格式化都跳过，结束后归还原裁剪框。 */
static void draw_result_panel(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e);lv_layer_t *l=lv_event_get_layer(e);
    lv_area_t saved;
    if(!ui_underlay_begin(l,p->opaque_cover,&saved)) return;
    draw_result_content(e);
    l->_clip_area=saved;
}

/* 底部按钮的父对象是整屏，因此Y319的触点不会被较短的导航背景截断。
 * 相邻按钮外扩4像素仍保留间隔；只放宽下方，不修改任何触摸坐标。 */
static void bottom_hit(lv_event_t *e)
{
    lv_hit_test_info_t *h=lv_event_get_hit_test_info(e); lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    h->res=h->point->x>=a.x1-4&&h->point->x<=a.x2+4&&h->point->y>=a.y1-4&&h->point->y<=319;
}
/* 图标16px+8px间距+实际文字宽度组成一个整体，在按钮内居中。
 * 两字、四字及“测量中...”都走同一规则，不再分别硬编码文字偏移。 */
static void bottom_layout(lv_obj_t *button)
{
    lv_obj_t *label=lv_obj_get_child(button,0);
    lv_obj_align(label,LV_ALIGN_CENTER,12,0); /* (16px图标+8px间距)/2，使组合中心不偏。 */
}
/* 底部三键沿用参考图布局，背景/图标原生绘制，文字和状态由LVGL控件管理。 */
static void bottom_icon(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e);lv_obj_t *o=lv_event_get_target_obj(e);
    lv_area_t a;lv_obj_get_coords(o,&a);lv_layer_t *l=lv_event_get_layer(e);
    /* 按钮自己的圆角背景已由LVGL画好，这里只叠加透明的状态提示与图标。 */
    /* 状态遮罩沿用同一个胶囊圆角，按下或禁用时也不会把圆弧涂成方角。 */
    if(lv_obj_has_state(o,LV_STATE_PRESSED))ui_rect(l,a.x1,a.y1,lv_area_get_width(&a),lv_area_get_height(&a),0x2D7DD3,25,LV_RADIUS_CIRCLE,0,0);
    if(lv_obj_has_state(o,LV_STATE_DISABLED))ui_rect(l,a.x1,a.y1,lv_area_get_width(&a),lv_area_get_height(&a),0xCDD7E4,95,LV_RADIUS_CIRCLE,0,0);
    /* 页脚采用原尺寸细轮廓：盾牌勾、带刻度直尺、完整齿轮；播放键略小。
     * 先测量当前标签再定位图标，宽度变化时两者仍作为一组居中。 */
    const char *symbol=o==p->start_button?(p->automatic?UI_FOOTER_STOP:UI_FOOTER_PLAY):
        o==p->navigation[3]?UI_FOOTER_SETTINGS:p->quality_mode?UI_FOOTER_MEASURE:UI_FOOTER_INSPECT;
    lv_point_t size;lv_obj_t *label=lv_obj_get_child(o,0);
    lv_text_get_size(&size,lv_label_get_text(label),&ui_font_14_bold,0,0,LV_COORD_MAX,0);
    int x=a.x1+(lv_area_get_width(&a)-size.x-24)/2;
    int y=a.y1+(lv_area_get_height(&a)-ui_footer_icons_16.line_height)/2;
    ui_text(l,x,y,16,&ui_footer_icons_16,o==p->start_button?UI_WHITE:UI_INK,symbol,LV_TEXT_ALIGN_LEFT);
}
static void nav_click(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e); lv_obj_t *o=lv_event_get_target_obj(e);
    if(o==p->navigation[0]) measurement_page_navigate(p,p->quality_mode?0:1);
    else if(o==p->navigation[3]) measurement_page_navigate(p,3);
    else if(o==p->standard_button) { if(p->navigation_changed) p->navigation_changed(p->application_context,4); }
    else measurement_page_navigate(p,2);
}
static void footer_draw(lv_event_t *e)
{
    /* 页脚留白与主背景一致，按钮自己的圆角渐变/阴影负责边缘融合。 */
    ui_gradient(lv_event_get_layer(e),0,274,480,46,0xFCFBF9,0xF5F5F1,0,0);
}
static void start_click(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e);
    if(p->primary_action) p->primary_action(p->application_context); else measurement_page_start(p);
}
static void zoom_icon(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e);lv_obj_t *o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);
    ui_icon(lv_event_get_layer(e),a.x1,a.y1+3,16,UI_INK,
            o==p->zoom[0]?UI_ICON_ZOOM_IN:o==p->zoom[1]?UI_ICON_ZOOM_OUT:UI_ICON_FIT);
}
static void zoom_click(lv_event_t *e)
{
    MeasurementPage *p=lv_event_get_user_data(e); lv_obj_t *o=lv_event_get_target_obj(e);
    uint32_t s=p->geometry.scale_q8;
    if(o==p->zoom[2]) s=GEOMETRY_DEFAULT_SCALE_Q8;
    else if(o==p->zoom[0]) s=LV_MIN(4096U,s+s/4U);
    else s=LV_MAX(128U,s-s/5U);
    measurement_page_set_scale(p,s);
}
static lv_obj_t *make_button(lv_obj_t *parent,int x,int y,int w,int h,const char *s,bool blue)
{
    lv_obj_t *o=lv_button_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    /* 蓝色启停按钮高36px，两端使用18px半径，保持完整半圆。
     * 圆角与下方渐变图片的裁剪共用这一尺寸，避免边缘使用不同的轮廓。 */
    lv_obj_set_style_radius(o,blue?h/2:LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_opa(o,255,0);
    lv_obj_set_style_bg_color(o,ui_color(blue?0xB6E0FF:0xFFFFFF),0);
    lv_obj_set_style_bg_grad_color(o,ui_color(blue?UI_CYAN:0xFBFBF8),0);
    lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);
    /* 蓝色按钮的浅色1px描边会覆盖渐变的最外沿，尤其下沿形成明显平直亮线，
     * 与较蓝的底部渐变叠加后产生色差。去掉这层描边，让完整圆角渐变直接
     * 以抗锯齿边缘收尾；白色按钮仍保留原边框，文字、点击范围和阴影不变。 */
    lv_obj_set_style_border_width(o,blue?0:1,0);lv_obj_set_style_border_color(o,ui_color(UI_BORDER),0);
    /* 按下态的两个端点一起改变，避免只改顶部却残留另一套渐变。 */
    lv_obj_set_style_bg_color(o,ui_color(0xCAE6FA),LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(o,ui_color(0xA9D2F0),LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(o,ui_color(0xD1DCEA),LV_STATE_DISABLED);
    lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_NONE,LV_STATE_DISABLED);
    /* 页脚的静态渐变预计算为抖色565，避免原生565竖向渐变出现明显横带。
     * 图片仍使用按钮原圆角/阴影/边框；禁用态恢复原纯灰，按下态有独立渐变。 */
    if(h==36) {
        lv_obj_set_style_bg_image_src(o,ui_skin_gradient_source(w,h,blue?0xB6E0FF:0xFFFFFF,blue?UI_CYAN:0xFBFBF8),0);
        lv_obj_set_style_bg_image_src(o,ui_skin_gradient_source(w,h,0xCAE6FA,0xA9D2F0),LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
        lv_obj_set_style_bg_image_opa(o,LV_OPA_COVER,0);
        lv_obj_set_style_bg_image_opa(o,LV_OPA_TRANSP,LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_STATE_DISABLED);
    }
    lv_obj_set_style_text_color(o,ui_color(blue?UI_WHITE:UI_INK),0);
    lv_obj_set_style_text_font(o,&ui_font_14_bold,0);
    lv_obj_set_style_shadow_width(o,8,0);lv_obj_set_style_shadow_color(o,ui_color(0xBFD9EC),0);
    lv_obj_set_style_shadow_opa(o,65,0);lv_obj_set_style_shadow_offset_y(o,3,0);
    lv_obj_t *label=lv_label_create(o);lv_label_set_text(label,s);lv_obj_center(label);
    return o;
}
static void sync_ui(MeasurementPage *p)
{
    /* 不在每次进度/结果到达时重排被覆盖的控件；恢复时会按session最新值补一次。 */
    if(p->presentation_paused) return;
    MeasurementSession *s=&p->session;
    bool mode_changed=!p->ui_synced || p->geometry.quality_mode!=p->quality_mode;
    bool automatic_changed=p->geometry.automatic!=p->automatic;
    const char *button=p->quality_mode?(p->automatic?"停止质检":"启动质检"):
        measurement_session_busy(s)?"测量中...":"开始测量";
    if(strcmp(lv_label_get_text(p->start_label),button)) {
        lv_label_set_text(p->start_label,button);bottom_layout(p->start_button);
    }
    lv_obj_t *mode_label=lv_obj_get_child(p->navigation[0],0);
    const char *mode_text=p->quality_mode?"测量":"质检";
    if(strcmp(lv_label_get_text(mode_label),mode_text)) {
        lv_label_set_text(mode_label,mode_text);bottom_layout(p->navigation[0]);
    }
    lv_obj_set_hidden(p->standard_button,!p->quality_mode||p->selected_navigation>=2);
    /* 自动质检进行时，中键必须仍可点，以便随时停止；普通测量忙时禁止重复请求。 */
    lv_obj_set_state(p->start_button,LV_STATE_DISABLED,p->selected_navigation>=2||
        (!p->automatic&&(measurement_session_busy(s)||!s->connected||!s->calibrated||!p->backend.start)));
    lv_obj_set_state(p->navigation[0],LV_STATE_DISABLED,measurement_session_busy(s)||p->automatic);
    lv_obj_set_hidden(p->progress_bar,p->quality_mode);
    lv_obj_set_pos(p->result_panel,p->quality_mode?292:307,35);
    lv_obj_set_size(p->result_panel,p->quality_mode?183:168,p->quality_mode?240:237);
    lv_obj_set_pos(p->standard_button,292,35);lv_obj_set_size(p->standard_button,183,29);
    lv_obj_set_size(p->geometry.object,p->quality_mode?278:292,232);
    for(unsigned i=0;i<3;++i)lv_obj_set_pos(p->zoom[i],(p->quality_mode?191:206)+(int)i*28,7);

    /* 进度变化不重画不变的按钮；自动启停会换图标，模式会换导航图标，才显式失效。 */
    if(automatic_changed) lv_obj_invalidate(p->start_button);
    if(mode_changed) lv_obj_invalidate(p->navigation[0]);
    lv_obj_set_pos(p->navigation[2],134,176);lv_obj_set_size(p->navigation[2],46,21);
    lv_obj_set_hidden(p->navigation[2],!p->quality_mode);
    if(mode_changed||automatic_changed) {
        p->geometry.quality_mode=p->quality_mode;p->geometry.automatic=p->automatic;
        if(mode_changed) lv_obj_invalidate(p->geometry.object);
        else {
            lv_area_t a;lv_obj_get_coords(p->geometry.object,&a);
            a.x2=a.x1+195;a.y2=a.y1+52;lv_obj_invalidate_area(p->geometry.object,&a);
        }
    }
    lv_obj_set_pos(p->progress_bar,12,184);lv_obj_set_size(p->progress_bar,117,8);
    if(lv_bar_get_value(p->progress_bar)!=s->progress) lv_bar_set_value(p->progress_bar,s->progress,LV_ANIM_OFF);
    /* 相机状态由dashboard_network_update单独刷新，不随模拟测量结果变化。 */
    if(mode_changed || p->displayed.geometry!=s->geometry || p->displayed.state==(MeasurementState)-1) {
        lv_obj_invalidate(p->result_panel);
    } else if(p->quality_mode) {
        if(p->displayed.has_result!=s->has_result || memcmp(&s->result,&p->displayed.result,sizeof(s->result))) {
            const lv_area_t result={294,72,474,133};lv_obj_invalidate_area(p->result_panel,&result);
        }
    } else {
        bool values_changed=(p->displayed.state==MEASUREMENT_COMPLETE)!=(s->state==MEASUREMENT_COMPLETE) ||
            memcmp(&s->result,&p->displayed.result,sizeof(s->result));
        if(values_changed) {
            const lv_area_t values={312,67,472,160},residual={317,248,393,270};
            lv_obj_invalidate_area(p->result_panel,&values);lv_obj_invalidate_area(p->result_panel,&residual);
        }
        if(p->displayed.state!=s->state) {
            const lv_area_t state={317,192,462,215},bottom={398,248,473,270};
            lv_obj_invalidate_area(p->result_panel,&state);lv_obj_invalidate_area(p->result_panel,&bottom);
        }
        if(p->displayed.progress!=s->progress) {
            const lv_area_t progress={438,215,474,234};lv_obj_invalidate_area(p->result_panel,&progress);
        }
    }
    p->displayed=*s;p->ui_synced=true;
}
void measurement_page_create(MeasurementPage *p,lv_obj_t *screen,const MeasurementResult *initial)
{
    memset(p,0,sizeof(*p));ui_skin_init();
    measurement_session_init(&p->session,initial?initial->geometry:MEASUREMENT_RING,
        initial&&initial->connected,initial&&initial->calibrated,initial&&initial->simulated);
    p->displayed.state=(MeasurementState)-1;
    p->root=plain_object(screen,0,0,480,320);
    lv_obj_set_style_bg_color(p->root,ui_color(UI_BG),0);lv_obj_set_style_bg_opa(p->root,255,0);
    p->header=plain_object(p->root,0,0,480,30);lv_obj_add_event_cb(p->header,draw_header,LV_EVENT_DRAW_MAIN,p);
    geometry_view_create(&p->geometry,p->root);
    /* caption保留为主轮廓的状态容器，具体画面由geometry_view绘制。 */
    p->caption=plain_object(p->geometry.object,0,0,1,1);
    p->result_panel=plain_object(p->root,307,35,168,237);
    lv_obj_add_event_cb(p->result_panel,draw_result_panel,LV_EVENT_DRAW_MAIN,p);
    p->progress_bar=lv_bar_create(p->result_panel);lv_obj_remove_style_all(p->progress_bar);
    lv_obj_set_style_bg_opa(p->progress_bar,255,0);lv_obj_set_style_bg_color(p->progress_bar,ui_color(0xDCE9F7),0);
    lv_obj_set_style_radius(p->progress_bar,5,0);lv_obj_set_style_bg_opa(p->progress_bar,255,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(p->progress_bar,ui_color(0xA0D8FF),LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(p->progress_bar,ui_color(UI_CYAN),LV_PART_INDICATOR);
    /* 固定117×8横向蓝色渐变已离线抖为565，运行时只从Flash取像素。
     * LVGL的bar会按实际进度裁剪整幅渐变，并保留指示条圆角，不重新压缩色阶。 */
    lv_obj_set_style_bg_grad_dir(p->progress_bar,LV_GRAD_DIR_NONE,LV_PART_INDICATOR);
    lv_obj_set_style_bg_image_src(p->progress_bar,
        ui_skin_gradient_source(117,8,0xA0D8FF,UI_CYAN),LV_PART_INDICATOR);
    lv_obj_set_style_bg_image_opa(p->progress_bar,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_radius(p->progress_bar,5,LV_PART_INDICATOR);lv_bar_set_range(p->progress_bar,0,100);
    lv_obj_t *footer=plain_object(p->root,0,275,480,45);
    lv_obj_set_style_bg_color(footer,ui_color(UI_WHITE),0);lv_obj_set_style_bg_opa(footer,150,0);
    lv_obj_set_style_radius(footer,23,0);lv_obj_add_event_cb(footer,footer_draw,LV_EVENT_DRAW_MAIN,NULL);
    /* 36px高的完整胶囊在Y311收尾，下面仍留8px给阴影与触摸容错。 */
    p->navigation[0]=make_button(p->root,18,276,131,36,"质检",false);
    p->start_button=make_button(p->root,165,276,150,36,"开始测量",true);
    p->start_label=lv_obj_get_child(p->start_button,0);
    p->navigation[3]=make_button(p->root,332,276,131,36,"设置",false);
    lv_obj_t *bottom[]={p->navigation[0],p->start_button,p->navigation[3]};
    for(unsigned i=0;i<3;++i) {
        /* 按钮保留原触摸范围，只用原生背景替代带底色贴图。 */
        bottom_layout(bottom[i]);
        /* 可见底边Y311；外框11px足够搜索到Y319，最终由回调限制实际命中范围。 */
        lv_obj_set_ext_click_area(bottom[i],11);lv_obj_set_adv_hittest(bottom[i],true);
        lv_obj_add_event_cb(bottom[i],bottom_hit,LV_EVENT_HIT_TEST,NULL);
        lv_obj_add_event_cb(bottom[i],bottom_icon,LV_EVENT_DRAW_MAIN,p);
        lv_obj_add_event_cb(bottom[i],i==1?start_click:nav_click,LV_EVENT_CLICKED,p);
    }
    /* 记录小箭头是独立可点击对象，放在“最近检测记录”标题右侧。 */
    p->navigation[2]=make_button(p->result_panel,134,173,26,20,">",false);
    lv_obj_set_style_shadow_width(p->navigation[2],0,0);lv_obj_set_ext_click_area(p->navigation[2],2);
    lv_obj_add_event_cb(p->navigation[2],nav_click,LV_EVENT_CLICKED,p);
    p->standard_button=make_button(p->root,308,36,164,28,"标准设定   >",false);
    lv_obj_add_event_cb(p->standard_button,nav_click,LV_EVENT_CLICKED,p);
    lv_obj_set_style_bg_opa(p->standard_button,0,0);lv_obj_set_style_shadow_width(p->standard_button,0,0);
    lv_obj_set_style_border_width(p->standard_button,0,0);lv_obj_set_hidden(lv_obj_get_child(p->standard_button,0),true);
    lv_obj_set_style_bg_opa(p->navigation[2],0,0);lv_obj_set_style_shadow_width(p->navigation[2],0,0);
    lv_obj_set_style_border_width(p->navigation[2],0,0);lv_obj_set_hidden(lv_obj_get_child(p->navigation[2],0),true);
    for(unsigned i=0;i<3;++i) {
        p->zoom[i]=make_button(p->geometry.object,206+(int)i*28,7,25,24,"",false);
        lv_obj_set_style_radius(p->zoom[i],6,0);lv_obj_set_style_shadow_width(p->zoom[i],0,0);
        lv_obj_set_style_bg_opa(p->zoom[i],255,0);lv_obj_set_style_border_width(p->zoom[i],1,0);
        lv_obj_add_event_cb(p->zoom[i],zoom_click,LV_EVENT_CLICKED,p);
        lv_obj_add_event_cb(p->zoom[i],zoom_icon,LV_EVENT_DRAW_MAIN,p);
    }
    sync_ui(p);
}

void measurement_page_navigate(MeasurementPage * p, unsigned destination)
{
    if(!p || destination > 3 || p->selected_navigation == destination) return;
    p->selected_navigation = destination;
    if(destination < 2) p->quality_mode = destination == 1;
    /* 二级页是滑动覆盖层，主图必须留作动画底图；提前隐藏会出现空白/跳变。
     * 不透明菜单由LVGL正常遮挡裁剪，透明输入层负责防止触摸点穿。 */
    lv_obj_set_hidden(p->standard_button, destination >= 2 || !p->quality_mode);
    /* LVGL的位置/大小/隐藏更新会覆盖新旧区域；共同页头保持，不请求整屏切页。 */
    if(p->navigation_changed) p->navigation_changed(p->application_context, destination);
    sync_ui(p);
}

void measurement_page_set_units(MeasurementPage * p, bool inches)
{
    if(!p || p->inches == inches) return;
    p->inches = inches;
    p->displayed.state = (MeasurementState)-1;
    geometry_view_set_options(&p->geometry, p->geometry.show_grid, p->geometry.show_dimensions, p->geometry.show_contour, inches);
    sync_ui(p);
    lv_obj_invalidate(p->result_panel);
}

void measurement_page_set_calibration_capture(MeasurementPage * p, bool active)
{
    if(!p || p->calibration_capture_active == active) return;
    p->calibration_capture_active = active;
    p->displayed.state = (MeasurementState)-1; sync_ui(p);
}
void measurement_page_invalidate_result(MeasurementPage * p)
{
    if(!p || measurement_session_busy(&p->session)) return;
    p->session.state = MEASUREMENT_IDLE; p->session.error = MEASUREMENT_ERROR_NONE;
    p->session.has_result = false; p->session.progress = 0;
    p->session.result = (MeasurementResult){.geometry = p->session.geometry, .status = MEASUREMENT_PENDING};
    if(!p->presentation_paused) geometry_view_clear(&p->geometry);
    sync_ui(p);
}
void measurement_page_show_notice(MeasurementPage * p, const char * text, bool warning)
{
    if(!p || !text) return;
    snprintf(p->hint_text, sizeof(p->hint_text), "%s", text); p->notice_warning = warning;
    lv_obj_invalidate(p->result_panel);
}

void measurement_page_set_backend(MeasurementPage * p, const MeasurementBackend * backend)
{
    if(!p || measurement_session_busy(&p->session)) return;
    p->backend = backend ? *backend : (MeasurementBackend){0};
    sync_ui(p);
}

void measurement_page_pause_presentation(MeasurementPage *p,bool paused)
{
    if(!p || p->presentation_paused==paused) return;
    if(paused) {
        p->presentation_clock=p->clock_seconds;
        p->presentation_paused=true;
        return;
    }
    p->presentation_paused=false;
    /* 只在解除覆盖时同步最后的有效轮廓；期间没有丢掉任何会话结果。
     * has_result保留上一有效值，所以新一轮采集仍沿用既有的结果保持规则。 */
    if(p->session.has_result) geometry_view_update(&p->geometry,&p->session.result);
    else geometry_view_clear(&p->geometry);
    sync_ui(p);
    if(!p->quality_mode && p->presentation_clock!=p->clock_seconds) {
        const lv_area_t time={429,44,473,63};lv_obj_invalidate_area(p->result_panel,&time);
    }
}

bool measurement_page_start(MeasurementPage * p)
{
    if(!p || !p->root || p->selected_navigation >= 2 || measurement_session_busy(&p->session) || p->calibration_capture_active) return false;
    if(!measurement_session_start(&p->session, lv_tick_get())) { sync_ui(p); return false; }
    sync_ui(p); /* feedback is immediate, even before the backend returns */
    uint32_t id = p->session.request_id;
    if(!p->backend.start || !p->backend.start(p->backend.context, id, p->session.geometry)) {
        measurement_page_fail(p, id, MEASUREMENT_ERROR_BACKEND);
        return false;
    }
    return true;
}

bool measurement_page_select_geometry(MeasurementPage * p, MeasurementGeometry geometry)
{
    if(!p || !p->root || !measurement_session_select(&p->session, geometry)) return false;
    if(!p->presentation_paused && p->geometry.has_geometry && p->geometry.result.geometry != geometry)
        geometry_view_clear(&p->geometry);
    sync_ui(p);
    return true;
}

bool measurement_page_progress(MeasurementPage * p, uint32_t id, MeasurementState phase, uint8_t progress)
{
    measurement_page_process(p, lv_tick_get());
    if(!p || !measurement_session_progress(&p->session, id, phase, progress)) return false;
    sync_ui(p);
    return true;
}

bool measurement_page_complete(MeasurementPage * p, uint32_t id, const MeasurementResult * result)
{
    measurement_page_process(p, lv_tick_get());
    if(!p || !measurement_session_busy(&p->session) || p->session.request_id != id) return false;
    GeometryLayout layout;
    if(!geometry_layout_calculate(result, p->geometry.scale_q8, &layout)) {
        measurement_page_fail(p, id, MEASUREMENT_ERROR_RESULT);
        return false;
    }
    if(!measurement_session_complete(&p->session, id, result)) {
        if(p->backend.cancel) p->backend.cancel(p->backend.context, id);
        sync_ui(p);
        return false;
    }
    if(!p->presentation_paused) geometry_view_update(&p->geometry, &p->session.result);
    sync_ui(p);
    if(p->result_completed) p->result_completed(p->application_context, &p->session.result);
    return true;
}

bool measurement_page_fail(MeasurementPage * p, uint32_t id, MeasurementError error)
{
    if(!p || !measurement_session_fail(&p->session, id, error)) return false;
    if(p->backend.cancel) p->backend.cancel(p->backend.context, id);
    sync_ui(p);
    return true;
}

void measurement_page_set_device(MeasurementPage * p, bool connected, bool calibrated)
{
    if(!p) return;
    bool was_busy = measurement_session_busy(&p->session);
    measurement_session_set_device(&p->session, connected, calibrated);
    if(was_busy && !measurement_session_busy(&p->session) && p->backend.cancel)
        p->backend.cancel(p->backend.context, p->session.request_id);
    sync_ui(p);
}

void measurement_page_process(MeasurementPage * p, uint32_t now_ms)
{
    if(!p || !measurement_session_process(&p->session, now_ms)) return;
    if(p->backend.cancel) p->backend.cancel(p->backend.context, p->session.request_id);
    sync_ui(p);
}

bool measurement_page_set_scale(MeasurementPage * p, uint32_t scale)
{
    return p && geometry_view_set_scale(&p->geometry, scale);
}
