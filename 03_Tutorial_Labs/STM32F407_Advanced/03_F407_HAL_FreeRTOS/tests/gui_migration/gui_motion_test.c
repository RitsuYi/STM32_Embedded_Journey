#include "instrument/dashboard_ui.h"
#include "gui_demo.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}} while(0)
/* 使用正式页面/字体与真实LVGL输入、动画、52KiB池；只替代触摸坐标、时间和LCD。
 * 检查手势的实际结果，不通过伪造CLICKED或直接修改滚动标志来代替交互。 */
static uint32_t ms,frames;
/* 整屏副本只用于电脑截图，正式固件仍使用原来的两块32行缓冲。 */
static uint8_t pixels[480U*32U*2U],frame[480U*320U*2U];
static int32_t x,y;static bool down;
static MeasurementPage page;static InstrumentModel model;static DashboardUI ui;
static SystemMonitorSnapshot sample;
static uint32_t now(void) {return ms;}
void GUI_LVGL_AssertFailed(const char *file,int line)
{fprintf(stderr,"ASSERT %s:%d\n",file,line);exit(1);}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{
    CHECK(p&&a->x1>=0&&a->x2<480&&a->y1>=0&&a->y2<320);
    lv_draw_buf_t *active=lv_display_get_buf_active(d);
    for(int32_t row=a->y1;row<=a->y2;++row)
        for(int32_t col=a->x1;col<=a->x2;++col) {
            unsigned from=(unsigned)(row-a->y1)*active->header.stride+(unsigned)(col-a->x1)*2U;
            unsigned to=((unsigned)row*480U+(unsigned)col)*2U;
            frame[to]=p[from];frame[to+1]=p[from+1];
        }
    if(lv_display_flush_is_last(d))++frames;
    lv_display_flush_ready(d);
}
static void input(lv_indev_t *i,lv_indev_data_t *d)
{(void)i;d->point=(lv_point_t){x,y};d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static const SystemMonitorSnapshot *snapshot(void *context)
{(void)context;return &sample;}
static void advance(unsigned duration)
{
    for(unsigned i=0;i<duration;i+=5) {ms+=5;lv_timer_handler();CHECK(lv_mem_test()==LV_RESULT_OK);}
}
static void touch(int32_t px,int32_t py,bool pressed,unsigned duration)
{x=px;y=py;down=pressed;advance(duration);}
static void tap(lv_obj_t *o)
{
    lv_obj_update_layout(lv_screen_active());lv_area_t a;lv_obj_get_coords(o,&a);
    touch((a.x1+a.x2)/2,(a.y1+a.y2)/2,true,45);touch(x,y,false,45);
}
/* 保存真实条带合成结果，方便人工检查回弹空白和滑出遮挡，不能作为板上截图。 */
static void capture(const char *name)
{
    lv_refr_now(lv_display_get_default());FILE *f=fopen(name,"wb");CHECK(f);
    fprintf(f,"P6\n480 320\n255\n");
    for(unsigned i=0;i<480U*320U;++i) {
        uint16_t p=frame[2U*i]|(uint16_t)frame[2U*i+1U]<<8;
        fputc(((p>>11)&31U)*255U/31U,f);fputc(((p>>5)&63U)*255U/63U,f);fputc((p&31U)*255U/31U,f);
    }
    fclose(f);
}
/* direction=1为顶部向下拉，-1为底部向上推；到边后继续拖仍应随手指移动。
 * 按住不自动归位，松手后回到原边界，父页面和横向位置都不移动。 */
static void edges(lv_obj_t *o,const char *name)
{
    lv_obj_scroll_to_y(o,100000,LV_ANIM_OFF);advance(30);
    int32_t end=lv_obj_get_scroll_y(o);CHECK(end>0);
    for(int direction=1;direction>=-1;direction-=2) {
        int32_t edge=direction>0?0:end;
        lv_obj_scroll_to_y(o,edge,LV_ANIM_OFF);advance(30);
        touch(250,direction>0?150:250,true,45);
        for(unsigned i=0;i<8;++i)touch(x,y+direction*10,true,15);
        int32_t pulled=lv_obj_get_scroll_y(o);
        CHECK(direction>0?pulled<0:pulled>end);
        if(o==ui.body&&ui.menu==DASH_SETTINGS)
            capture(direction>0?"motion-settings-top.ppm":"motion-settings-bottom.ppm");
        advance(90);CHECK(lv_obj_get_scroll_y(o)==pulled);
        touch(x,y,false,15);advance(1000);
        CHECK(abs(lv_obj_get_scroll_y(o)-edge)<=1&&lv_obj_get_scroll_x(o)==0);
        CHECK(!ui.system.selected);
        printf("%s %s: dragged=%ld, settled=%ld\n",name,direction>0?"top":"bottom",
            (long)pulled,(long)lv_obj_get_scroll_y(o));
    }
}
/* 相同48px手指路程，改变采样之间的时间，证明惯性使用实际手势速度。
 * 返回松手80ms后的额外距离，不能只比较同一个固定动画有没有播放。 */
static int32_t flick(unsigned interval)
{
    lv_obj_scroll_to_y(ui.body,0,LV_ANIM_OFF);advance(40);
    touch(250,245,true,45);
    for(unsigned i=0;i<16;++i)touch(x,y-3,true,interval);
    int32_t released=lv_obj_get_scroll_y(ui.body);
    touch(x,y,false,15);advance(80);
    int32_t distance=lv_obj_get_scroll_y(ui.body)-released;advance(1200);
    CHECK(ui.menu==DASH_RECORDS);return distance;
}
static void check_close(void)
{
    /* 系统信息是设置的子页，原返回语义是先回设置，再退出到主页。 */
    if(ui.menu==DASH_DEVICE) {tap(ui.back);advance(40);CHECK(ui.menu==DASH_SETTINGS);}
    tap(ui.back);
    CHECK(!ui.closing&&!ui.animating&&lv_obj_is_hidden(ui.panel));
    CHECK(lv_obj_get_x(ui.panel)==480&&!page.presentation_paused);
    CHECK(!lv_obj_is_clickable(ui.transition_guard)&&page.selected_navigation==ui.last_main);
    /* 重复关闭应无副作用，也不能留下延迟执行的退出回调。 */
    dashboard_close(&ui);advance(200);
    CHECK(lv_obj_is_hidden(ui.panel)&&lv_obj_get_x(ui.panel)==480);
}
int main(void)
{
    lv_init();lv_tick_set_cb(now);lv_display_t *display=lv_display_create(480,320);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);lv_indev_t *pointer=lv_indev_create();
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,input);
    lv_timer_set_period(lv_indev_get_read_timer(pointer),15);
    instrument_defaults(&model);MeasurementResult initial={.geometry=MEASUREMENT_RECTANGLE,
        .status=MEASUREMENT_VALID,.connected=true,.simulated=true,.primary_cmm=3200,.secondary_cmm=1800};
    for(unsigned i=0;i<8;++i)CHECK(instrument_add_record(&model,&initial));
    measurement_page_create(&page,lv_screen_active(),&initial);GUI_DemoInit(&page,&model);
    dashboard_create(&ui,&page,&model,NULL);ui.system.read=snapshot;
    sample=(SystemMonitorSnapshot){.available=true,.complete=true,.count=6,.total_tasks=6};
    for(unsigned i=0;i<6;++i) {sample.tasks[i].id=i+1U;snprintf(sample.tasks[i].name,sizeof(sample.tasks[i].name),"Task%u",i);}
    advance(80);dashboard_open(&ui,DASH_SETTINGS);advance(240);
    edges(ui.body,"settings");CHECK(ui.menu==DASH_SETTINGS);check_close();
    dashboard_open(&ui,DASH_RECORDS);advance(240);edges(ui.body,"records");
    int32_t slow=flick(60),fast=flick(15);
    printf("same 48px gesture, extra distance after release: slow=%ld fast=%ld\n",(long)slow,(long)fast);
    CHECK(fast>slow&&slow>=0);check_close();
    dashboard_open(&ui,DASH_DEVICE);advance(240);tap(ui.system.tabs[1]);
    lv_area_t header;lv_obj_get_coords(ui.system.tabs[1],&header);
    edges(ui.system.list,"tasks-six");lv_area_t after;lv_obj_get_coords(ui.system.tabs[1],&after);
    CHECK(after.y1==header.y1&&lv_obj_get_scroll_y(ui.body)==0);
    tap(ui.system.tabs[3]);edges(ui.system.content,"device");check_close();
    /* 在回弹中重新按下并反向拖动，旧动画必须立即交还控制，不应卡住。 */
    dashboard_open(&ui,DASH_SETTINGS);advance(240);
    touch(250,150,true,45);for(unsigned i=0;i<7;++i)touch(x,y+10,true,15);
    touch(x,y,false,15);advance(30);CHECK(lv_obj_get_scroll_y(ui.body)<0);
    touch(250,220,true,30);int32_t held=lv_obj_get_scroll_y(ui.body);advance(45);
    CHECK(lv_obj_get_scroll_y(ui.body)==held);
    for(unsigned i=0;i<8;++i)touch(x,y-10,true,15);
    CHECK(lv_obj_get_scroll_y(ui.body)>held);touch(x,y,false,15);advance(1000);
    /* 关闭时仍有惯性也必须终止；再次打开从顶部开始。 */
    touch(250,240,true,45);for(unsigned i=0;i<8;++i)touch(x,y-8,true,15);
    touch(x,y,false,15);dashboard_close(&ui);advance(240);
    CHECK(!lv_indev_get_scroll_obj(pointer));dashboard_open(&ui,DASH_SETTINGS);advance(240);
    CHECK(lv_obj_get_scroll_y(ui.body)==0&&lv_obj_get_x(ui.panel)==0);
    /* 进入途中返回必须立即关闭；随后重开仍保留原180ms进入动画。 */
    dashboard_close(&ui);advance(240);dashboard_open(&ui,DASH_SETTINGS);advance(45);
    CHECK(lv_obj_get_x(ui.panel)>0&&lv_obj_get_x(ui.panel)<480);
    dashboard_close(&ui);CHECK(lv_obj_is_hidden(ui.panel)&&!ui.animating&&!ui.closing);
    CHECK(!page.presentation_paused&&lv_obj_get_x(ui.panel)==480);
    dashboard_open(&ui,DASH_RECORDS);advance(45);
    CHECK(ui.animating&&lv_obj_get_x(ui.panel)>0&&lv_obj_get_x(ui.panel)<480);advance(220);
    CHECK(!ui.animating&&!ui.closing&&!lv_obj_is_hidden(ui.panel)&&lv_obj_get_x(ui.panel)==0);
    CHECK(ui.menu==DASH_RECORDS&&page.presentation_paused);check_close();
    /* 菜单遮挡期间收到的结果，直接关闭时必须立即同步到主页。
     * 使用公开会话/完成接口，只替代相机结果，不直接篡改控件显示快照。 */
    dashboard_open(&ui,DASH_RECORDS);advance(240);
    page.session.connected=true;page.session.calibrated=true;
    CHECK(measurement_session_start(&page.session,ms));
    uint32_t count=model.record_count;
    MeasurementResult completed=initial;completed.geometry=page.session.geometry;
    completed.calibrated=true;completed.primary_cmm=4012;
    CHECK(measurement_page_complete(&page,page.session.request_id,&completed));
    CHECK(model.record_count==count+1U&&ui.summary_pending&&page.presentation_paused);
    dashboard_close(&ui);CHECK(lv_obj_is_hidden(ui.panel)&&!page.presentation_paused&&!ui.summary_pending);
    CHECK(page.displayed.state==MEASUREMENT_COMPLETE&&page.displayed.result.primary_cmm==4012);
    dashboard_open(&ui,DASH_SETTINGS);advance(240);check_close();
    lv_mem_monitor_t before;lv_mem_monitor(&before);
    for(unsigned i=0;i<20;++i) {dashboard_open(&ui,DASH_SETTINGS);advance(240);dashboard_close(&ui);advance(240);}
    lv_mem_monitor_t end;lv_mem_monitor(&end);CHECK(end.free_size>=before.free_size);
    unsigned idle=frames;advance(400);CHECK(frames<=idle+1U);
    printf("PASS: 8 edges, speed, interruption, immediate close/reopen, entry animation, covered result, idle redraw, 20 cycles; peak=%lu free=%lu\n",
        (unsigned long)end.max_used,(unsigned long)end.free_size);
    dashboard_destroy(&ui);lv_deinit();return 0;
}
