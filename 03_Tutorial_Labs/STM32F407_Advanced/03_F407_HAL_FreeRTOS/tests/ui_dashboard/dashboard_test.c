#include "instrument/dashboard_ui.h"
#include "gui_camera_config.h"
#include "gui_version.h"
#include "gui_demo.h"
#include "lvgl_port.h"
#include "ST7796.h"
#include "fonts/measurement_fonts.h"
#include "measurement/ui_draw.h"
#include "measurement/ui_icons.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "src/display/lv_display_private.h"
#include "src/draw/lv_draw_private.h"
#include "assets/ui_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}} while(0)
/* 使用真实LVGL、页面、模拟后端和触摸适配；只替代LCD与实际手指。
 * 307200字节截图缓冲仅在电脑测试程序存在，绝不编入F407固件。 */
static uint16_t frame[480*320];
static uint32_t now;
static int32_t tx,ty;
static bool down;
static MeasurementPage page;
static InstrumentModel model;
static DashboardUI ui;
/* 模拟DMA延迟到Finish才读取源数据，能发现提前归还/换序或下一条带覆盖缓冲的错误。 */
static const uint8_t *in_flight;
static uint8_t flight_copy[65534];
static uint16_t flight_x,flight_y,flight_w,flight_h;
static unsigned dma_starts,dma_finishes,draw_while_dma;
static uint64_t sent_bytes;
static bool fail_start,fail_finish;
/* 仅测试计时契约时注入虚拟绘图/末块等待时间，不把电脑执行速度当作F407速度。 */
static unsigned finish_delay_ms, render_delay_ms;
/* 行数对照只记录真实绘图回调/发送量；不把主机或注入时间当作F407性能。 */
static unsigned geometry_draws, record_draws;
/* 直接观察真实LVGL创建的任务；只计算已变换图片与裁剪框交集的面积，
 * 用来证明少做了被遮挡的圆环/柔影工作，不将面积或电脑运行时间冒充实机提速。 */
static bool probe_tasks, enforce_cover;
static int32_t probe_limit;
static uint64_t ring_work;
static unsigned submitted_tasks;
void __real_lv_draw_finalize_task_creation(lv_layer_t *layer,lv_draw_task_t *task);
void __wrap_lv_draw_finalize_task_creation(lv_layer_t *layer,lv_draw_task_t *task)
{
    if(probe_tasks) {
        ++submitted_tasks;
        if(task->type==LV_DRAW_TASK_TYPE_IMAGE) {
            const lv_draw_image_dsc_t *d=task->draw_dsc;
            if(d->src==&ui_metal_ring || d->src==&ui_metal_shadow) {
                lv_area_t a;
                if(enforce_cover)CHECK(task->clip_area.x2<probe_limit);
                a.x1=LV_MAX(task->_real_area.x1,task->clip_area.x1);
                a.y1=LV_MAX(task->_real_area.y1,task->clip_area.y1);
                a.x2=LV_MIN(task->_real_area.x2,task->clip_area.x2);
                a.y2=LV_MIN(task->_real_area.y2,task->clip_area.y2);
                if(a.x1<=a.x2 && a.y1<=a.y2)ring_work+=lv_area_get_size(&a);
            }
        }
    }
    __real_lv_draw_finalize_task_creation(layer,task);
}
#define FULL_FRAME_BLOCKS ((320U + GUI_DRAW_BUFFER_ROWS - 1U) / GUI_DRAW_BUFFER_ROWS)
uint32_t HAL_GetTick(void) {return now;}
void GUI_LVGL_AssertFailed(const char *file,int line)
{lv_mem_monitor_t m;lv_mem_monitor(&m);fprintf(stderr,"LVGL assert %s:%d menu=%u edit=%u time=%lu free=%lu biggest=%lu peak=%lu\n",file,line,ui.menu,ui.edit,(unsigned long)now,(unsigned long)m.free_size,(unsigned long)m.free_biggest_size,(unsigned long)m.max_used);exit(1);}
HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint8_t *pixels,uint32_t stride)
{
    CHECK(x+w<=480&&y+h<=320&&stride>=w*2U);
    for(unsigned row=0;row<h;++row)memcpy(frame+(y+row)*480+x,pixels+row*stride,w*2U);
    return HAL_OK;
}
HAL_StatusTypeDef ST7796_StartRGB565DMA(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint8_t *pixels)
{
    CHECK(!in_flight&&x+w<=480&&y+h<=320&&(uint32_t)w*h*2U<=65534U);
    if(fail_start){fail_start=false;return HAL_ERROR;}
    in_flight=pixels;flight_x=x;flight_y=y;flight_w=w;flight_h=h;
    memcpy(flight_copy,pixels,(unsigned)w*h*2U);++dma_starts;sent_bytes+=(unsigned)w*h*2U;
    return HAL_OK;
}
HAL_StatusTypeDef ST7796_FinishRGB565DMA(void)
{
    CHECK(in_flight);CHECK(!memcmp(in_flight,flight_copy,(unsigned)flight_w*flight_h*2U));
    now += finish_delay_ms;
    if(!fail_finish)for(unsigned y=0;y<flight_h;++y)for(unsigned x=0;x<flight_w;++x) {
        unsigned i=(y*flight_w+x)*2U;
        frame[(flight_y+y)*480+flight_x+x]=(uint16_t)(in_flight[i]<<8|in_flight[i+1]);
    }
    in_flight=NULL;++dma_finishes;
    if(fail_finish){fail_finish=false;return HAL_TIMEOUT;}
    return HAL_OK;
}
static void observe_overlap(lv_event_t *e)
{
    (void)e;
    ++geometry_draws;
    if(in_flight) {++draw_while_dma;CHECK(!memcmp(in_flight,flight_copy,(unsigned)flight_w*flight_h*2U));}
}
static bool reader(void *context,int32_t *x,int32_t *y,bool *pressed)
{(void)context;*x=tx;*y=ty;*pressed=down;return true;}
static void advance(unsigned ms)
{for(unsigned i=0;i<ms;i+=5){now+=5;measurement_page_process(&page,now);lv_timer_handler();GUI_DisplayService();}}
static void tap_at(lv_obj_t *o,int x,int y)
{
    lv_obj_update_layout(lv_screen_active());lv_point_t p={x,y};
    if(lv_indev_search_obj(lv_screen_active(),&p)!=o)fprintf(stderr,"tap mismatch %d,%d expected=%p actual=%p edit=%u menu=%u\n",x,y,(void *)o,(void *)lv_indev_search_obj(lv_screen_active(),&p),ui.edit,ui.menu);
    CHECK(lv_indev_search_obj(lv_screen_active(),&p)==o);
    tx=x;ty=y;down=true;advance(60);down=false;advance(230);
}
static void tap(lv_obj_t *o)
{
    CHECK(o);lv_obj_update_layout(lv_screen_active());lv_area_t a;lv_obj_get_coords(o,&a);
    tap_at(o,(a.x1+a.x2)/2,(a.y1+a.y2)/2);
}
static void incremental_contract(const char *phase);
static void simulate_render_cost(lv_event_t *event)
{
    (void)event;
    now += render_delay_ms;
    render_delay_ms = 0U;
}
static void frame_stats_contract(void)
{
    /* 在创建业务页面/动画定时器之前，直接验证真实LVGL刷新事件和延迟DMA收尾：
     * 多块整屏只能记1帧；CPU绘图7ms加每块1ms的Finish，须全部计入耗时。
     * 块数由当前行数计算，32行时为10块；这些虚拟毫秒不是实机性能。 */
    lv_display_t *display = lv_display_get_default();
    GUI_DisplayStats before, after;
    lv_refr_now(display);
    GUI_DisplayGetStats(&before);
    lv_obj_add_event_cb(lv_screen_active(),simulate_render_cost,LV_EVENT_DRAW_MAIN,NULL);
    render_delay_ms = 7U;
    finish_delay_ms = 1U;
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
    finish_delay_ms = 0U;
    lv_obj_remove_event_cb(lv_screen_active(),simulate_render_cost);
    GUI_DisplayGetStats(&after);
    CHECK(after.completed_frames == before.completed_frames + 1U);
    CHECK(after.last_frame_blocks == FULL_FRAME_BLOCKS && after.last_frame_bytes == 307200U);
    CHECK(after.last_frame_ms == 7U + FULL_FRAME_BLOCKS && after.max_frame_ms >= 7U + FULL_FRAME_BLOCKS);
    CHECK(after.last_frame_wait_ms==FULL_FRAME_BLOCKS && after.last_frame_pack_ms==0U);
    before = after;
    lv_refr_now(display); /* 无失效区域，不能将空刷新记作1帧。 */
    GUI_DisplayGetStats(&after);
    CHECK(after.completed_frames == before.completed_frames);

    /* 先结束旧统计窗口，再在明确的1秒窗口内提交3帧，应为3.0，不能按DMA块数计帧。 */
    now += 1000U;
    GUI_DisplayService();
    for(unsigned i = 0; i < 3U; ++i) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(display);
    }
    now += 1000U;
    GUI_DisplayService();
    GUI_DisplayGetStats(&after);
    CHECK(after.fps_x10 == 30U);
    now += 1000U;
    GUI_DisplayService();
    GUI_DisplayGetStats(&after);
    CHECK(after.fps_x10 == 0U);
    printf("PASS: FPS counts completed refreshes; %u blocks + 7ms synthetic render = %ums; idle = 0 FPS\n",
           FULL_FRAME_BLOCKS,7U+FULL_FRAME_BLOCKS);
}
static void screenshot(const char *path)
{
    incremental_contract(path);
    lv_obj_invalidate(page.root);advance(45);FILE *f=fopen(path,"wb");CHECK(f);
    fprintf(f,"P6\n480 320\n255\n");
    for(unsigned i=0;i<480U*320U;++i){uint16_t p=frame[i];
        fputc(((p>>11)&31)*255/31,f);fputc(((p>>5)&63)*255/63,f);fputc((p&31)*255/31,f);}
    fclose(f);
}
/* 截图函数会强制完整重绘，不能用它证明精确脏区。这里先保留增量帧，再与同一时刻
 * 的完整重绘逐像素比较；旧文字清除、状态变化和菜单揭露均不能出现残影。 */
static void incremental_contract(const char *phase)
{
    lv_display_t *display=lv_display_get_default();GUI_DisplayService();lv_refr_now(display);
    uint16_t *before=malloc(sizeof(frame));CHECK(before);memcpy(before,frame,sizeof(frame));
    lv_obj_invalidate(page.root);lv_refr_now(display);
    unsigned count=0;for(unsigned i=0;i<480U*320U;++i)if(before[i]!=frame[i]) {
        if(count<5)fprintf(stderr,"incremental %s mismatch %u,%u: %04x/%04x\n",phase,i%480,i/480,before[i],frame[i]);
        ++count;
    }
    free(before);CHECK(count==0);
}
static void asynchronous_contract(void)
{
    lv_display_t *d=lv_display_get_default();
    GUI_DisplayStats before, after;
    GUI_DisplayGetStats(&before);
    unsigned first=dma_starts;uint64_t bytes=sent_bytes;
    lv_obj_invalidate(lv_screen_active());lv_refr_now(d);
    CHECK(dma_starts-first==FULL_FRAME_BLOCKS&&sent_bytes-bytes==307200&&!in_flight);
    GUI_DisplayGetStats(&after);
    CHECK(after.completed_frames == before.completed_frames + 1U);
    /* 启动失败和完成超时都模拟丢一块；Service期限过后必须恢复完整画面。 */
    uint16_t *reference=malloc(sizeof(frame));CHECK(reference);memcpy(reference,frame,sizeof(frame));
    for(unsigned failure=0;failure<2;++failure) {
        GUI_DisplayGetStats(&before);
        memset(frame,0,sizeof(frame));fail_start=failure==0;fail_finish=failure==1;
        lv_obj_invalidate(page.root);lv_refr_now(d);CHECK(!in_flight);
        GUI_DisplayGetStats(&after);
        CHECK(after.completed_frames == before.completed_frames);
        CHECK(after.transfer_errors == before.transfer_errors + 1U);
        now+=100;GUI_DisplayService();lv_refr_now(d);CHECK(!memcmp(reference,frame,sizeof(frame)));
    }
    free(reference);printf("PASS: full screen %u DMA blocks/307200 bytes; failure full repaint recovery\n",FULL_FRAME_BLOCKS);
}
/* 不变数据不得触发SPI；运行时钟只刷新自己的小矩形，不能带动整头/结果卡。 */
static void dirty_area_contract(void)
{
    lv_display_t *d=lv_display_get_default();lv_refr_now(d);uint64_t start=sent_bytes;
    for(unsigned i=0;i<5;++i) {
        measurement_page_set_device(&page,page.session.connected,page.session.calibrated);
        measurement_page_set_backend(&page,&page.backend);
        geometry_view_set_options(&page.geometry,page.geometry.show_grid,page.geometry.show_dimensions,
                                  page.geometry.show_contour,page.geometry.inches);
        lv_refr_now(d);
    }
    CHECK(sent_bytes==start);advance(1000);uint64_t bytes=sent_bytes-start;
    CHECK(bytes>0&&bytes<=5000);
    incremental_contract("clock only");
    printf("PASS: unchanged data = 0 SPI bytes; quality clock one second = %llu bytes\n",(unsigned long long)bytes);
}
static void animation_contract(void)
{
    /* 从真实导航入口开菜单，先验证X=480时底图仍完整；只检查结束帧会漏掉提前隐藏。 */
    lv_display_t *d=lv_display_get_default();lv_refr_now(d);
    uint16_t *underlay=malloc(sizeof(frame));CHECK(underlay);memcpy(underlay,frame,sizeof(frame));
    measurement_page_navigate(&page,3);
    CHECK(!lv_obj_is_hidden(page.geometry.object)&&!lv_obj_is_hidden(page.result_panel));
    CHECK(lv_obj_get_x(ui.panel)==480&&lv_obj_is_clickable(ui.transition_guard));
    lv_refr_now(d);
    for(unsigned y=36;y<268;++y)CHECK(!memcmp(underlay+y*480,frame+y*480,480*2));
    free(underlay);
    for(unsigned i=0;i<6;++i){
        advance(30);incremental_contract("menu sliding in");
        lv_point_t point={20,290};
        if(lv_obj_get_x(ui.panel)>20)CHECK(lv_indev_search_obj(page.root,&point)==ui.transition_guard);
    }
    CHECK(lv_obj_get_x(ui.panel)==0);
    /* 返回只提交最终主页：无需推进虚拟时钟就应结束菜单状态，第一轮增量
     * 刷新必须完整；同一时刻再次刷新/重复返回不能再产生滑出中间帧。 */
    GUI_DisplayStats before,after;GUI_DisplayGetStats(&before);
    dashboard_close(&ui);
    CHECK(lv_obj_is_hidden(ui.panel)&&!lv_obj_is_clickable(ui.transition_guard));
    CHECK(!ui.animating&&!ui.closing&&!page.presentation_paused&&lv_anim_get(ui.panel,NULL)==NULL);
    lv_refr_now(d);GUI_DisplayGetStats(&after);
    CHECK(after.completed_frames==before.completed_frames+1U);
    before=after;dashboard_close(&ui);lv_refr_now(d);GUI_DisplayGetStats(&after);
    CHECK(after.completed_frames==before.completed_frames);
    incremental_contract("direct return first frame");
    /* 在滑入中途返回也立即关闭，重开从屏幕右侧正常入场。
     * 原入场动画必须已取消，不能在稍后把关闭菜单或新菜单移错位置。 */
    measurement_page_navigate(&page,3);advance(40);
    int x=lv_obj_get_x(ui.panel);CHECK(x>0&&x<480);
    dashboard_close(&ui);CHECK(lv_obj_is_hidden(ui.panel)&&lv_anim_get(ui.panel,NULL)==NULL);
    CHECK(!page.presentation_paused&&!lv_obj_is_clickable(ui.transition_guard));
    incremental_contract("return during entry");
    dashboard_open(&ui,DASH_SETTINGS);CHECK(lv_obj_get_x(ui.panel)==480);
    /* 动画目标仍180ms；33ms刷新定时器可能在下一次采样才执行终点回调。 */
    advance(250);
    if(lv_obj_get_x(ui.panel)!=0||!lv_obj_is_clickable(ui.transition_guard))
        fprintf(stderr,"reopened x=%ld panel_hidden=%u guard_hidden=%u closing=%u\n",
                (long)lv_obj_get_x(ui.panel),lv_obj_is_hidden(ui.panel),!lv_obj_is_clickable(ui.transition_guard),ui.closing);
    CHECK(lv_obj_get_x(ui.panel)==0&&lv_obj_is_clickable(ui.transition_guard));
    dashboard_close(&ui);advance(250);
    CHECK(lv_obj_is_hidden(ui.panel)&&!lv_obj_is_clickable(ui.transition_guard)&&lv_anim_get(ui.panel,NULL)==NULL);
    puts("PASS: 180ms entry preserves underlay/input guard; direct return uses one refresh, interrupts/reopens safely");
}
static void padded_flush_contract(void)
{
    /* 主机直接调用真实flush，构造3像素/行但stride=8的两行缓冲。
     * 默认固件stride对齐为1，普通画面不会自然产生padding，必须单独验证这条分支。 */
    lv_display_t *d=lv_display_get_default();GUI_DisplayService();
    lv_draw_buf_t padded={0};lv_draw_buf_t *saved=d->buf_act;
    uint8_t pixels[]={0x00,0xF8,0xE0,0x07,0x1F,0x00,0xAA,0xBB,
                      0xFF,0xFF,0x00,0x00,0xE0,0xFF,0xCC,0xDD};
    const uint16_t expected[]={0xF800,0x07E0,0x001F,0xFFFF,0x0000,0xFFE0};
    padded.header.cf=LV_COLOR_FORMAT_RGB565;padded.header.stride=8;
    d->buf_act=&padded;d->flushing=1;d->flushing_last=1;
    const lv_area_t a={10,20,12,21};uint64_t bytes=sent_bytes;
    d->flush_cb(d,&a,pixels);CHECK(!in_flight&&!d->flushing&&sent_bytes-bytes==12);
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<3;++x)CHECK(frame[(20+y)*480+10+x]==expected[y*3+x]);
    d->buf_act=saved;d->flushing_last=0;
    puts("PASS: real flush compacts padded RGB565 in place, exact byte order and 12-byte contiguous payload");
}
static void plot_dither_contract(void)
{
    static const uint8_t threshold[8][8]={
        {0,48,12,60,3,51,15,63},{32,16,44,28,35,19,47,31},
        {8,56,4,52,11,59,7,55},{40,24,36,20,43,27,39,23},
        {2,50,14,62,1,49,13,61},{34,18,46,30,33,17,45,29},
        {10,58,6,54,9,57,5,53},{42,26,38,22,41,25,37,21}};
    /* 无网格/文字覆盖的实际雾蓝像素，与原8位平方渐变的末级抖色数学值逐像素对照。
     * 覆盖8×8所有相位，验证生成表、行随机寻址和LVGL565显示链路没有再量化。 */
    unsigned width=page.quality_mode?278U:292U;
    for(unsigned sy=92;sy<112;++sy)for(unsigned sx=50;sx<66;++sx) {
        unsigned x=sx-8,y=sy-36,top=(231-y)*(231-y)*256/(231*231),bottom=y*y*256/(231*231);
        unsigned edge=(unsigned)abs((int)(2*x-width+1))*256/width,t=threshold[sy&7][sx&7];
        unsigned rgb[]={205+(top*38+bottom*10+edge*2)/256,222+(top*23+bottom*7+edge*2)/256,
                        236+(top*15+bottom*4+edge)/256};
        unsigned levels[]={31,63,31},out[3];
        for(unsigned i=0;i<3;++i) {
            unsigned scaled=rgb[i]*levels[i];out[i]=scaled/255+(scaled%255*64>t*255+127);
        }
        CHECK(frame[sy*480+sx]==(uint16_t)(out[0]<<11|out[1]<<5|out[2]));
    }
}
/* 只在电脑记录当前结论卡片像素，验证自动采集时不会把上一份数字闪成“--”。
 * 不比较时钟、进度等应当变化的区域，也不为F407增加截图内存。 */
static void retain_result_contract(uint16_t *saved,bool capture)
{
    lv_obj_invalidate(page.result_panel);lv_refr_now(lv_display_get_default());
    for(unsigned y=0;y<39;++y) {
        const uint16_t *row=frame+(94+y)*480+296;
        if(capture)memcpy(saved+y*174,row,174*2);
        else CHECK(!memcmp(saved+y*174,row,174*2));
    }
}
/* 检查实际蓝色按钮上下边缘轮廓：下圆角应与上圆角对称，不能被一条平直裁切线截断。
 * 用蓝色通道优势识别填充，允许抗锯齿与RGB565抖色带来最多2px的边界差。 */
static void footer_contract(void)
{
    lv_area_t a;lv_obj_get_coords(page.start_button,&a);
    for(int dy=1;dy<7;++dy) {
        int top=0,bottom=0;
        for(int x=a.x1;x<=a.x2;++x) {
            uint16_t t=frame[(a.y1+dy)*480+x],b=frame[(a.y2-dy)*480+x];
            top+=((t&31U)>((t>>11)&31U)+4U);
            bottom+=((b&31U)>((b>>11)&31U)+4U);
        }
        CHECK(top>90&&abs(top-bottom)<=2);
    }
}
/* 通过真实LVGL绘图和当前flush捕获纯色，检查白色及RGB565分量。
 * 这些是电脑上的数字像素检查，不代表液晶面板亮度/伽马已经实测正确。 */
static void color_contract(void)
{
    const uint32_t colors[]={0xFFFFFF,0xFF0000,0x00FF00,0x0000FF,0x000000};
    const uint16_t expected[]={0xFFFF,0xF800,0x07E0,0x001F,0x0000};
    lv_obj_t *group=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(group);
    lv_obj_set_size(group,240,24);lv_obj_set_scrollable(group,false);
    for(unsigned i=0;i<5;++i) {
        lv_obj_t *o=lv_obj_create(group);lv_obj_remove_style_all(o);
        lv_obj_set_pos(o,(int)i*40,0);lv_obj_set_size(o,40,24);
        lv_obj_set_style_bg_color(o,ui_color(colors[i]),0);lv_obj_set_style_bg_opa(o,255,0);
    }
    lv_refr_now(lv_display_get_default());
    for(unsigned i=0;i<5;++i)for(unsigned y=0;y<24;++y)for(unsigned x=0;x<40;++x)
        CHECK(frame[y*480+i*40+x]==expected[i]);
    lv_obj_delete(group);
}
/* 任意动态渐变仍由真实LVGL565绘制，检查单调色阶和原生中性灰量化值。
 * 静态敏感渐变另由plot_dither_contract与条带/裁剪一致性检查，不再要求全局末级抖色。
 * 该检查针对送屏数字像素，不能代替面板本身的伽马/刷新速度测量。 */
static void gradient_contract(void)
{
    lv_obj_t *o=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(o);
    lv_obj_set_size(o,64,128);lv_obj_set_scrollable(o,false);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xA0C0E0),0);
    lv_obj_set_style_bg_grad_color(o,lv_color_hex(0xB0D0F0),0);
    lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);lv_obj_set_style_bg_opa(o,255,0);
    lv_refr_now(lv_display_get_default());
    unsigned distinct=0,last=0;
    for(unsigned y=0;y<128;y+=8) {
        unsigned sum=0;for(unsigned yy=y;yy<y+8;++yy)for(unsigned x=0;x<64;++x)sum+=(frame[yy*480+x]>>11)&31U;
        CHECK(sum>=last);distinct+=(sum>last);last=sum;
    }
    /* 任意运行时渐变由原生565绘制；静态预计算背景的抖色另按精确像素检查。 */
    CHECK(distinct>=3);
    lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_NONE,0);lv_obj_set_style_bg_color(o,lv_color_hex(0x808080),0);
    lv_refr_now(lv_display_get_default());
    unsigned red=0,green=0,blue=0;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
        uint16_t c=frame[y*480+x];unsigned r=c>>11,g=(c>>5)&63U,b=c&31U;
        CHECK(r==16&&g==32&&b==16);red+=r;green+=g;blue+=b;
    }
    CHECK(red==blue&&green==2048);lv_obj_delete(o);
    puts("PASS: native RGB565 colors, gray, gradient monotonicity; static dither checked separately");
}
/* 固定同一时刻，分别用1/7/8/10/16/20/32行双缓冲重绘，再裁成不对齐的小矩形重绘。
 * 整幅RGB565必须逐字节一致，才能排除步幅、解码起点或局部刷新引入的方块/接缝。 */
static void redraw_contract(void)
{
    lv_display_t *display=lv_display_get_default();
    GUI_DisplayService();
    uint8_t *pixels1=display->buf_1->data,*pixels2=display->buf_2->data;
    /* 主机独立缓冲用于对比，固件容量由GUI_DRAW_BUFFER_ROWS决定。 */
    uint8_t *test_pixels=malloc(2U*480U*32U*2U);CHECK(test_pixels);
    lv_obj_invalidate(page.root);lv_refr_now(display);
    uint16_t *reference=malloc(sizeof(frame));CHECK(reference);memcpy(reference,frame,sizeof(frame));
    const unsigned heights[]={1,7,8,10,16,20,32};
    for(unsigned i=0;i<sizeof(heights)/sizeof(heights[0]);++i) {
        lv_display_set_buffers(display,test_pixels,test_pixels+480U*32U*2U,480U*2U*heights[i],LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_obj_invalidate(page.root);lv_refr_now(display);
        if(memcmp(reference,frame,sizeof(frame))) {
            unsigned count=0;int x1=480,y1=320,x2=0,y2=0;
            for(unsigned p=0;p<480U*320U;++p)if(reference[p]!=frame[p]) {
                int x=(int)(p%480),y=(int)(p/480);x1=LV_MIN(x1,x);y1=LV_MIN(y1,y);x2=LV_MAX(x2,x);y2=LV_MAX(y2,y);
                if(count<8)fprintf(stderr,"pixel %d,%d expected=%04x actual=%04x\n",x,y,reference[p],frame[p]);
                ++count;
            }
            fprintf(stderr,"RGB565 mismatch at strip height %u: %u pixels, area %d,%d..%d,%d\n",heights[i],count,x1,y1,x2,y2);
        }
        CHECK(!memcmp(reference,frame,sizeof(frame)));
    }
    const lv_area_t areas[]={{17,39,87,79},{83,81,139,205},{143,88,261,247},{313,73,466,158}};
    for(unsigned i=0;i<sizeof(areas)/sizeof(areas[0]);++i) {
        lv_obj_invalidate_area(page.root,&areas[i]);lv_refr_now(display);
        CHECK(!memcmp(reference,frame,sizeof(frame)));
    }
    CHECK(!in_flight);
    lv_display_set_buffers(display,pixels1,pixels2,GUI_DRAW_BUFFER_BYTES,LV_DISPLAY_RENDER_MODE_PARTIAL);
    free(test_pixels);free(reference);puts("PASS: RGB565 1/7/8/10/16/20/32-row double-buffer and cropped redraw consistency");
}
static void progress_gradient_contract(void)
{
    /* 使用真实页面的bar，不复制控件实现：100%作为固定渐变基准，部分进度必须
     * 裁剪同一批像素；以原8位设计颜色+8×8抖色的独立数学期望检查色阶。 */
    lv_display_t *d=lv_display_get_default();lv_obj_t *bar=page.progress_bar;
    int32_t saved=lv_bar_get_value(bar);lv_bar_set_value(bar,100,LV_ANIM_OFF);lv_refr_now(d);
    lv_area_t a;lv_obj_get_coords(bar,&a);CHECK(lv_area_get_width(&a)==117&&lv_area_get_height(&a)==8);
    uint16_t reference[8*117];
    for(unsigned y=0;y<8;++y)memcpy(reference+y*117,frame+(a.y1+y)*480+a.x1,117*2);
    static const uint8_t bayer[8][8]={
        {0,48,12,60,3,51,15,63},{32,16,44,28,35,19,47,31},
        {8,56,4,52,11,59,7,55},{40,24,36,20,43,27,39,23},
        {2,50,14,62,1,49,13,61},{34,18,46,30,33,17,45,29},
        {10,58,6,54,9,57,5,53},{42,26,38,22,41,25,37,21}};
    for(unsigned y=2;y<6;++y)for(unsigned x=5;x<112;++x) {
        unsigned mix=x*255U/116U,t=bayer[(a.y1+y)&7][(a.x1+x)&7];
        unsigned first[]={160,216,255},last[]={152,203,250},levels[]={31,63,31},out[3];
        for(unsigned c=0;c<3;++c) {
            unsigned channel=((first[c]*(255U-mix)+last[c]*mix)*0x8081U)>>23;
            unsigned scaled=channel*levels[c];out[c]=scaled/255U+(scaled%255U*64U>t*255U+127U);
        }
        CHECK(reference[y*117+x]==(uint16_t)(out[0]<<11|out[1]<<5|out[2]));
    }
    const unsigned progress[]={0,1,2,5,25,50,75,99,100};
    for(unsigned i=0;i<sizeof(progress)/sizeof(progress[0]);++i) {
        lv_bar_set_value(bar,progress[i],LV_ANIM_OFF);lv_refr_now(d);
        unsigned width=117U*progress[i]/100U;
        for(unsigned y=2;y<6;++y)for(unsigned x=5;x+5<width;++x)
            CHECK(frame[(a.y1+y)*480+a.x1+x]==reference[y*117+x]);
        incremental_contract("progress gradient clipping");
    }
    uint8_t saved_progress=page.session.progress;page.session.progress=50;
    lv_bar_set_value(bar,50,LV_ANIM_OFF);lv_obj_invalidate(page.result_panel);lv_refr_now(d);
    screenshot("progress_gradient.ppm");redraw_contract();
    page.session.progress=saved_progress;lv_bar_set_value(bar,saved,LV_ANIM_OFF);
    lv_obj_invalidate(page.result_panel);lv_refr_now(d);
    puts("PASS: fixed blue gradient exact dither pixels, 0/1/2/5/25/50/75/99/100% clipping and round ends");
}
static void observe_record_draw(lv_event_t *event)
{(void)event;++record_draws;}
static void strip_comparison_contract(void)
{
    /* 两个实际场景分别比较8/16/20/32行；画面、时间和滚动位置完全相同。
     * 新增32行核对画质和重复准备次数，不能据此宣称MCU耗时已下降。 */
    lv_display_t *d=lv_display_get_default();uint8_t *old1=d->buf_1->data,*old2=d->buf_2->data;
    uint8_t *buffers=malloc(2U*480U*32U*2U);uint16_t *reference=malloc(sizeof(frame));CHECK(buffers&&reference);
    const unsigned rows[]={8,16,20,32};
    for(unsigned scene=0;scene<2;++scene) {
        if(scene) {
            dashboard_open(&ui,DASH_RECORDS);advance(220);lv_obj_scroll_to_y(ui.body,73,LV_ANIM_OFF);
            for(unsigned i=0;i<lv_obj_get_child_count(ui.body);++i)
                lv_obj_add_event_cb(lv_obj_get_child(ui.body,i),observe_record_draw,LV_EVENT_DRAW_MAIN,NULL);
        }
        lv_refr_now(d);memcpy(reference,frame,sizeof(frame));
        unsigned previous=UINT32_MAX;
        for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);++i) {
            lv_display_set_buffers(d,buffers,buffers+480U*32U*2U,480U*rows[i]*2U,LV_DISPLAY_RENDER_MODE_PARTIAL);
            unsigned blocks=dma_starts,draws=scene?record_draws:geometry_draws;uint64_t bytes=sent_bytes;
            if(scene)lv_obj_invalidate(ui.body);else lv_obj_invalidate(page.root);
            lv_refr_now(d);CHECK(!in_flight&&!memcmp(reference,frame,sizeof(frame)));
            unsigned calls=(scene?record_draws:geometry_draws)-draws;CHECK(calls>0&&calls<previous);previous=calls;
            printf("STRIP_COMPARE scene=%s rows=%u blocks=%u bytes=%llu draw_callbacks=%u\n",
                   scene?"records":"main",rows[i],dma_starts-blocks,(unsigned long long)(sent_bytes-bytes),calls);
        }
    }
    lv_display_set_buffers(d,old1,old2,GUI_DRAW_BUFFER_BYTES,LV_DISPLAY_RENDER_MODE_PARTIAL);
    free(buffers);free(reference);dashboard_close(&ui);advance(220);
    puts("PASS: 8/16/20/32-row comparison identical pixels; fewer repeated draw callbacks");
}
static void record_cache_update_contract(void)
{
    /* 真正发起测量，再在尚未完成时打开历史。完成插入新记录后，已显示行的缓存
     * 也必须换成新编号；否则滚动会显示旧文字配新结论，属于可见的数据错误。 */
    measurement_page_navigate(&page,0);CHECK(measurement_page_start(&page));advance(350);
    CHECK(measurement_session_busy(&page.session));measurement_page_navigate(&page,2);advance(250);
    char old_id[12];memcpy(old_id,ui.record_text[0].id,sizeof(old_id));
    advance(2000);CHECK(page.session.state==MEASUREMENT_COMPLETE);
    CHECK(strcmp(old_id,ui.record_text[0].id)!=0);
    incremental_contract("history completion cache");
    uint16_t *saved=malloc(sizeof(frame));CHECK(saved);memcpy(saved,frame,sizeof(frame));
    dashboard_open(&ui,DASH_RECORDS);lv_refr_now(lv_display_get_default());
    CHECK(!memcmp(saved,frame,sizeof(frame)));free(saved);
    dashboard_close(&ui);advance(250);
    puts("PASS: measurement completing under history updates cached text and preserves exact rendering");
}
static unsigned connects,disconnects;
static bool connect_request(void *context,const char *ssid,const char *password)
{(void)context;CHECK(!strcmp(ssid,"Lab_WiFi"));CHECK(!strcmp(password,"demo12345"));++connects;return true;}
static bool disconnect_request(void *context){(void)context;++disconnects;return true;}
static unsigned scans;
static bool scan_request(void *context) {(void)context;++scans;return true;}
/* 通过正式编辑器提交，测试时只替换网络队列，不绕过GUI的连接回调。 */
static void request_lab_wifi(void)
{
    lv_obj_send_event(ui.rows[0],LV_EVENT_CLICKED,NULL);
    lv_textarea_set_text(ui.textarea,"Lab_WiFi");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    lv_textarea_set_text(ui.textarea,"demo12345");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    CHECK(ui.connecting&&!ui.password[0]);
}
static void wifi_scan_contract(void)
{
    /* 真实LVGL验证刷新、扫描中禁用、5条列表、滚动选择和密码输入。
     * 普通数据快照替代串口，不能当作实际无线扫描证据。 */
    DashboardNetworkOps ops={.connect=connect_request,.disconnect=disconnect_request,.scan=scan_request};
    CameraNetworkState state={.online=1,.wifi=CAMERA_WIFI_OFFLINE};
    dashboard_set_network(&ui,&ops);dashboard_network_update(&ui,&state);
    dashboard_open(&ui,DASH_NETWORK);advance(250);tap(ui.rows[2]);
    CHECK(scans==1&&ui.wireless.scan==CAMERA_SCAN_WAITING);
    CHECK(lv_obj_has_state(ui.rows[2],LV_STATE_DISABLED));
    state.scan=CAMERA_SCAN_RUNNING;dashboard_network_update(&ui,&state);
    lv_obj_t *same=ui.rows[2];dashboard_network_update(&ui,&state);CHECK(same==ui.rows[2]);
    state.scan=CAMERA_SCAN_DONE;state.count=CAMERA_AP_LIMIT;
    for(unsigned i=0;i<CAMERA_AP_LIMIT;++i) {
        snprintf(state.ap[i].ssid,sizeof(state.ap[i].ssid),"AP%u",i);
        state.ap[i].rssi=(int8_t)(-30-(int)i);state.ap[i].secured=1;
    }
    snprintf(state.ap[0].ssid,sizeof(state.ap[0].ssid),"Lab_WiFi");
    snprintf(state.ap[1].ssid,sizeof(state.ap[1].ssid),"\xF0\xA0\x80\x80");
    dashboard_network_update(&ui,&state);advance(50);
    CHECK(!lv_obj_has_state(ui.rows[2],LV_STATE_DISABLED));
    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(ui.ap_rows[1],0)),"\\u20000"));
    screenshot("wifi_scan.ppm");
    lv_obj_scroll_to_view(ui.ap_rows[CAMERA_AP_LIMIT-1],LV_ANIM_OFF);advance(30);
    CHECK(lv_obj_get_scroll_y(ui.body)>0);
    lv_obj_scroll_to_view(ui.ap_rows[0],LV_ANIM_OFF);advance(30);tap(ui.ap_rows[0]);
    CHECK(ui.edit&&!strcmp(ui.target_ssid,"Lab_WiFi"));
    state.wifi=CAMERA_WIFI_CONNECTED;snprintf(state.ssid,sizeof(state.ssid),"Other_AP");
    snprintf(state.ip,sizeof(state.ip),"192.0.2.5");dashboard_network_update(&ui,&state);
    CHECK(!strcmp(ui.target_ssid,"Lab_WiFi"));
    unsigned before=connects;lv_textarea_set_text(ui.textarea,"demo12345");
    lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);CHECK(connects==before+1&&!ui.password[0]);
    /* 旧热点/普通未连/短暂错误不能覆盖用户目标；7秒真实成功立即结束等待。 */
    CHECK(ui.connecting&&!strcmp(ui.connecting_ssid,"Lab_WiFi"));
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.body,0)),"Lab_WiFi"));
    lv_obj_scroll_to_y(ui.body,0,LV_ANIM_OFF);screenshot("wifi_connecting.ppm");
    state.wifi=CAMERA_WIFI_OFFLINE;state.error=CAMERA_NET_REJECTED;
    dashboard_network_update(&ui,&state);advance(7000);
    CHECK(ui.connecting&&!ui.connected);
    state.wifi=CAMERA_WIFI_CONNECTED;state.error=0;
    snprintf(state.ssid,sizeof(state.ssid),"Lab_WiFi");dashboard_network_update(&ui,&state);
    CHECK(ui.connected&&!ui.connecting&&!ui.connect_requested);
    /* 无新状态时仍须到期；后台继续尝试也不能重开已过15秒的界面等待。 */
    request_lab_wifi();state.wifi=CAMERA_WIFI_CONNECTING;dashboard_network_update(&ui,&state);
    advance(14500);CHECK(ui.connecting);
    advance(600);CHECK(!ui.connecting&&ui.connect_requested&&strstr(ui.message,"超时"));
    screenshot("wifi_connect_timeout.ppm");
    state.wifi=CAMERA_WIFI_OFFLINE;dashboard_network_update(&ui,&state);
    state.wifi=CAMERA_WIFI_CONNECTING;dashboard_network_update(&ui,&state);CHECK(!ui.connecting);
    state.wifi=CAMERA_WIFI_CONNECTED;dashboard_network_update(&ui,&state);
    CHECK(ui.connected&&!ui.connecting&&!ui.connect_requested); /* 迟到成功仍是真实状态。 */
    request_lab_wifi();
    lv_obj_send_event(ui.rows[1],LV_EVENT_CLICKED,NULL);CHECK(!ui.connecting);
    for(unsigned i=0;i<16;++i) {
        state.count=(i&1U)?CAMERA_AP_LIMIT:0;state.scan=CAMERA_SCAN_DONE;
        dashboard_network_update(&ui,&state);advance(10);
    }
    state.count=0;state.scan=CAMERA_SCAN_FAILED;state.error=CAMERA_NET_INCOMPLETE;
    dashboard_network_update(&ui,&state);CHECK(ui.wireless.count==0);
    request_lab_wifi(); /* 实际失联应取消尚未到15秒的等待，不能继续假装相机可用。 */
    memset(&state,0,sizeof(state));dashboard_network_update(&ui,&state);advance(30);
    CHECK(!ui.connecting&&!ui.connect_requested);
    /* 未握手就点刷新必须立即反馈，不能发出扫描或把按钮永久置为忙。 */
    unsigned before_scan=scans;
    lv_obj_scroll_to_view(ui.rows[2],LV_ANIM_OFF);advance(30);tap(ui.rows[2]);
    CHECK(scans==before_scan&&ui.wireless.scan==CAMERA_SCAN_FAILED);
    CHECK(!lv_obj_has_state(ui.rows[2],LV_STATE_DISABLED));
    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(ui.rows[2],1)),"扫描暂不可用"));
    dashboard_set_network(&ui,NULL);dashboard_close(&ui);advance(250);
    puts("PASS: Wi-Fi scan/SSID isolation; pending target survives offline, success at 7s, timeout at 15s, late success and cancel");
}
static bool reject_save(void *context,const InstrumentModel *m){(void)context;(void)m;return false;}
static bool silent_backend(void *context,uint32_t id,MeasurementGeometry geometry)
{(void)context;(void)id;(void)geometry;return true; /* 模拟已受理但不回结果的相机。 */}

static void reopen_transfer_contract(void)
{
    /* 进入动画尚未推进、菜单完全在屏外时，重开本身不应重发主页。
     * 只比较相同虚拟时刻的真实flush字节数，不用电脑运行时间冒充MCU速度。
     * 同时覆盖两种主页、设置/记录，以及完整进入和中途返回后的再次打开。 */
    MeasurementBackend original=page.backend,quiet={.start=silent_backend};
    unsigned prior_navigation=page.selected_navigation;
    measurement_page_set_backend(&page,&quiet);
    lv_display_t *d=lv_display_get_default();
    const DashboardMenu menus[]={DASH_SETTINGS,DASH_RECORDS};
    for(unsigned mode=0;mode<2;++mode)for(unsigned menu=0;menu<2;++menu)for(unsigned interrupted=0;interrupted<2;++interrupted) {
        measurement_page_navigate(&page,mode);advance(40);
        dashboard_open(&ui,menus[menu]);advance(interrupted?40:250);
        dashboard_close(&ui);lv_refr_now(d); /* 先发送应有的返回画面，不算入下一次进入。 */
        uint64_t before=sent_bytes;
        dashboard_open(&ui,menus[menu]);lv_refr_now(d);
        uint64_t reopened=sent_bytes-before;
        printf("REOPEN mode=%u menu=%u interrupted=%u initial_bytes=%llu\n",
               mode,(unsigned)menus[menu],interrupted,(unsigned long long)reopened);
        CHECK(lv_obj_get_x(ui.panel)==480&&ui.animating&&page.presentation_paused);
        CHECK(reopened==0U);
        advance(40);CHECK(lv_obj_get_x(ui.panel)>0&&lv_obj_get_x(ui.panel)<480);
        incremental_contract("reopen original entry intermediate");
        advance(250);CHECK(lv_obj_get_x(ui.panel)==0&&!ui.animating);
        dashboard_close(&ui);
        CHECK(lv_obj_is_hidden(ui.panel)&&lv_anim_get(ui.panel,NULL)==NULL);
        CHECK(!page.presentation_paused&&!lv_obj_is_clickable(ui.transition_guard));
        incremental_contract("reopen direct return final");
    }
    measurement_page_set_backend(&page,&original);
    measurement_page_navigate(&page,prior_navigation);advance(40);
    puts("PASS: 8 reopen cases send zero pre-animation pixels; original entry and direct return remain correct");
}

static void occlusion_contract(void)
{
    /* 同一真实菜单位置分别关闭/开启应用层裁剪：最终全屏必须逐像素相同。
     * 同时统计真实圆环/柔影任务的可绘包围面积，不按一份自造的简化画面测性能。 */
    lv_display_t *d=lv_display_get_default();MeasurementBackend original=page.backend;
    MeasurementBackend quiet={.start=silent_backend};measurement_page_set_backend(&page,&quiet);
    uint16_t *reference=malloc(sizeof(frame));CHECK(reference);
    uint64_t before_work=0,after_work=0;unsigned before_tasks=0,after_tasks=0;
    const int positions[]={0,37,95,180,291,470,480};
    for(unsigned mode=0;mode<2;++mode)for(unsigned shape=0;shape<2;++shape) {
        measurement_page_navigate(&page,mode);
        MeasurementGeometry g=shape?MEASUREMENT_RECTANGLE:MEASUREMENT_RING;
        CHECK(measurement_page_select_geometry(&page,g));CHECK(measurement_page_start(&page));
        MeasurementResult r=page.session.result;r.geometry=g;r.status=MEASUREMENT_VALID;
        r.primary_cmm=shape?3200:3012;r.secondary_cmm=shape?2000:1206;
        r.connected=true;r.calibrated=true;r.simulated=true;
        CHECK(measurement_page_complete(&page,page.session.request_id,&r));
        measurement_page_navigate(&page,3);
        /* 仅测试固定位置，不让定时器改变动画进度；收尾仍走正式关闭流程。 */
        lv_anim_delete(ui.panel,NULL);
        for(unsigned i=0;i<sizeof(positions)/sizeof(positions[0]);++i) {
            lv_obj_set_x(ui.panel,positions[i]);lv_obj_update_layout(page.root);
            page.opaque_cover=NULL;page.geometry.opaque_cover=NULL;
            ring_work=0;submitted_tasks=0;probe_tasks=true;
            lv_obj_invalidate(page.root);lv_refr_now(d);probe_tasks=false;
            uint64_t old_work=ring_work;unsigned old_tasks=submitted_tasks;
            memcpy(reference,frame,sizeof(frame));
            page.opaque_cover=ui.panel;page.geometry.opaque_cover=ui.panel;
            ring_work=0;submitted_tasks=0;probe_limit=positions[i];
            enforce_cover=positions[i]>0 && positions[i]<480;probe_tasks=true;
            lv_obj_invalidate(page.root);lv_refr_now(d);probe_tasks=false;enforce_cover=false;
            CHECK(!memcmp(reference,frame,sizeof(frame)));
            CHECK(ring_work<=old_work && submitted_tasks<=old_tasks);
            before_work+=old_work;after_work+=ring_work;
            before_tasks+=old_tasks;after_tasks+=submitted_tasks;
            if(mode==1 && shape==0)printf("OCCLUSION x=%d ring_area=%llu/%llu tasks=%u/%u\n",
                positions[i],(unsigned long long)old_work,(unsigned long long)ring_work,old_tasks,submitted_tasks);
            if(positions[i]==95) {
                redraw_contract();
                if(mode==1)screenshot(shape?"option_a_rectangle_mid.ppm":"option_a_ring_mid.ppm");
            }
        }
        dashboard_close(&ui);advance(250);incremental_contract("occlusion close");
    }
    CHECK(after_work<before_work && after_tasks<before_tasks);
    printf("PASS: all overlay positions/modes/shapes identical; image work area %llu -> %llu, tasks %u -> %u\n",
        (unsigned long long)before_work,(unsigned long long)after_work,before_tasks,after_tasks);
    free(reference);measurement_page_set_backend(&page,&original);
}

static void presentation_contract(void)
{
    /* 使用静默后端手动送真实进度/完成事件，覆盖菜单期间与直接返回之后。
     * 模型必须立即接收，返回首帧已有最新状态，返回后的结果也不能被继续冻结。 */
    MeasurementBackend original=page.backend,quiet={.start=silent_backend};
    measurement_page_set_backend(&page,&quiet);measurement_page_navigate(&page,1);
    CHECK(measurement_page_select_geometry(&page,MEASUREMENT_RING));
    CHECK(measurement_page_start(&page));uint32_t id=page.session.request_id;
    CHECK(measurement_page_progress(&page,id,MEASUREMENT_ACQUIRING,10));advance(40);
    uint32_t old_count=model.inspected;MeasurementResult old_geometry=page.geometry.result;
    measurement_page_navigate(&page,2);advance(30);CHECK(ui.animating&&page.presentation_paused);
    DashboardRecordText old_text=ui.record_text[0];
    uint8_t shown_progress=page.displayed.progress;
    CHECK(measurement_page_progress(&page,id,MEASUREMENT_FITTING,75));
    CHECK(page.session.progress==75 && page.displayed.progress==shown_progress);
    MeasurementResult r=page.session.result;r.geometry=MEASUREMENT_RING;r.status=MEASUREMENT_VALID;
    r.primary_cmm=3022;r.secondary_cmm=1206;r.connected=true;r.calibrated=true;r.simulated=true;
    CHECK(measurement_page_complete(&page,id,&r));
    CHECK(model.inspected==old_count+1U && page.total_count==old_count);
    CHECK(page.geometry.result.primary_cmm==old_geometry.primary_cmm);
    CHECK(!memcmp(&old_text,&ui.record_text[0],sizeof(old_text))&&ui.menu_dirty&&ui.summary_pending);
    incremental_contract("result arrives while entering");
    advance(250);CHECK(!ui.animating&&!ui.menu_dirty&&page.presentation_paused);
    CHECK(strcmp(old_text.id,ui.record_text[0].id)!=0);
    CHECK(ui.record_text[0].verdict==model.records[0].verdict);
    CHECK(page.total_count==old_count); /* 被盖住的主页没有反复重绘统计。 */
    dashboard_close(&ui);CHECK(page.total_count==model.inspected&&!page.presentation_paused);
    CHECK(page.geometry.result.primary_cmm==3022&&page.displayed.result.primary_cmm==3022);
    CHECK(page.verdict==model.records[0].verdict&&!ui.summary_pending&&lv_obj_is_hidden(ui.panel));
    incremental_contract("latest covered result in first return frame");advance(30);
    CHECK(measurement_page_start(&page));id=page.session.request_id;
    CHECK(measurement_page_progress(&page,id,MEASUREMENT_FITTING,90));
    r.primary_cmm=3032;CHECK(measurement_page_complete(&page,id,&r));
    CHECK(model.inspected==old_count+2U&&page.total_count==model.inspected);
    CHECK(page.geometry.result.primary_cmm==3032 && page.displayed.result.primary_cmm==3032);
    incremental_contract("result arrives after direct return");advance(250);
    CHECK(!page.presentation_paused&&!ui.animating&&!ui.summary_pending);
    CHECK(!lv_obj_is_clickable(ui.transition_guard)&&page.total_count==model.inspected);
    CHECK(page.displayed.result.primary_cmm==3032&&page.geometry.result.primary_cmm==3032);
    incremental_contract("latest result after return");

    /* 被菜单盖住时超时处理仍执行；退出后看到真实失败状态，不能一直停在采集中。 */
    CHECK(measurement_page_start(&page));measurement_page_navigate(&page,3);
    measurement_page_process(&page,page.session.started_ms+page.session.timeout_ms+1U);
    CHECK(page.session.state==MEASUREMENT_FAILED);dashboard_close(&ui);advance(250);
    CHECK(page.displayed.state==MEASUREMENT_FAILED&&!page.presentation_paused);
    incremental_contract("timeout under overlay");
    measurement_page_set_backend(&page,&original);
    puts("PASS: progress/results/records/timeouts continue; direct return shows latest data and resumes updates");
}

static void menu_reuse_contract(void)
{
    dashboard_open(&ui,DASH_SETTINGS);advance(250);
    lv_obj_t *saved[7];for(unsigned i=0;i<7;++i)saved[i]=ui.rows[i];
    dashboard_close(&ui);advance(250);dashboard_network_status(&ui,true,"ReuseTest","192.0.2.9");
    dashboard_open(&ui,DASH_SETTINGS);advance(250);
    for(unsigned i=0;i<7;++i)CHECK(saved[i]==ui.rows[i]);
    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(ui.rows[0],1)),"ReuseTest"));
    dashboard_close(&ui);advance(250);dashboard_network_status(&ui,false,NULL,NULL);
    dashboard_open(&ui,DASH_RECORDS);advance(250);
    lv_obj_scroll_by(ui.body,0,-110,LV_ANIM_ON);advance(30);
    int32_t y=lv_obj_get_scroll_y(ui.body);dashboard_close(&ui);advance(30);
    CHECK(lv_obj_get_scroll_y(ui.body)==y);advance(250);
    incremental_contract("settings reuse and scroll stop");
    puts("PASS: seven settings rows reused with fresh values; list inertia stops on direct return");
}
/* 用真实触摸滑到底再进入，证明新增末行可到达，版本一致且不伪报联网/更新成功。
 * 后端连接/断开回报到来时，当前更新页必须立即显示新状态；不接真实服务器。 */
static void update_entry_contract(void)
{
    dashboard_network_status(&ui,false,NULL,NULL);dashboard_open(&ui,DASH_SETTINGS);advance(250);
    CHECK(ui.rows[6]&&!ui.rows[7]);
    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(ui.rows[6],0)),"检查更新"));
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.rows[6],1)),GUI_FIRMWARE_VERSION));
    tx=250;ty=245;down=true;advance(40);
    for(int y=245;y>=90;y-=10){ty=y;advance(20);}down=false;advance(300);
    CHECK(lv_obj_get_scroll_y(ui.body)>0);
    lv_area_t last,body;lv_obj_get_coords(ui.rows[6],&last);lv_obj_get_coords(ui.body,&body);
    CHECK(last.y1>=body.y1&&last.y2<=body.y2);
    screenshot("settings_update_bottom.ppm");tap(ui.rows[6]);
    CHECK(ui.menu==DASH_UPDATE&&!ui.connected&&!ui.connecting);
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.body,0)),GUI_FIRMWARE_VERSION));
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.body,1)),"未连接"));
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.body,2)),"暂未接入"));
    screenshot("update_offline.ppm");incremental_contract("update offline");
    dashboard_network_status(&ui,true,"OTA_Test","192.0.2.10");advance(60);
    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(ui.body,1)),"Wi-Fi 已连接"));
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.body,2)),"暂未接入"));
    screenshot("update_online_reserved.ppm");incremental_contract("update network changed");
    dashboard_network_status(&ui,false,NULL,NULL);advance(60);
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(ui.body,1)),"未连接"));
    tap(ui.back);CHECK(ui.menu==DASH_SETTINGS);dashboard_close(&ui);advance(250);
    puts("PASS: bottom update entry touch/version/back; live Wi-Fi states and honest OTA placeholder");
}
static void font_check(void)
{
    const char *strings[]={"质检合格标准设定当前检测结果累计统计最近检测记录智能视觉测量仪测量设置","网络密码设备信息目标上下偏差断开连接检查更新当前版本固件升级服务暂未接入连接中连接等待超时请检查网络状态联网接口未接入或请求忙"};
    for(unsigned f=0;f<2;++f)for(unsigned i=0;i<2;++i){
        const lv_font_t *font=f==0?&ui_font_12:&ui_font_14_bold;
        const char *p=strings[i];while(*p){uint32_t cp;unsigned char c=(unsigned char)*p++;
            if(c<128)cp=c;else{cp=c&15;cp=(cp<<6)|((unsigned char)*p++&63);cp=(cp<<6)|((unsigned char)*p++&63);}
            lv_font_glyph_dsc_t d;if(!lv_font_get_glyph_dsc(font,&d,cp,0)) {fprintf(stderr,"Missing glyph U+%04lx font %u\n",(unsigned long)cp,f);exit(1);}}
    }
    /* 图标必须来自独立字体的真实字形，不能由缺字占位框或中文fallback冒充。 */
    const uint32_t icons[]={0xE001,0xF013,0xF545,0xF3ED,0xF46C,0xF080,0xF15C,0xF1EB,0xF00E,
        0xF010,0xF065,0xF058,0xF057,0xF140,0xF04B,0xF04D,0xF054,0xF030};
    const lv_font_t *fonts[]={&ui_icons_16,&ui_icons_20};
    for(unsigned f=0;f<2;++f)for(unsigned i=0;i<sizeof(icons)/sizeof(icons[0]);++i) {
        lv_font_glyph_dsc_t d;CHECK(lv_font_get_glyph_dsc(fonts[f],&d,icons[i],0));
        CHECK(!d.is_placeholder&&d.resolved_font==fonts[f]&&d.box_w>0&&d.box_h>0);
        CHECK(d.ofs_x+(int)d.box_w<=(f?28:24)&&d.box_h<=fonts[f]->line_height);
    }
    /* 页脚五个轻轮廓字形必须完全落在16px方框内，不能依赖裁切掩盖超大图标。 */
    for(uint32_t cp=0xE101;cp<=0xE105;++cp) {
        lv_font_glyph_dsc_t d;CHECK(lv_font_get_glyph_dsc(&ui_footer_icons_16,&d,cp,0));
        CHECK(!d.is_placeholder&&d.resolved_font==&ui_footer_icons_16&&d.box_w&&d.box_h);
        CHECK(d.ofs_x>=0&&d.ofs_x+(int)d.box_w<=16&&d.box_h<=16&&d.adv_w<=16);
    }
    lv_point_t size;
    lv_text_get_size(&size,"30.12",&ui_font_26_bold,0,0,LV_COORD_MAX,0);CHECK(size.x<=74);
    lv_text_get_size(&size,"30.12",&ui_font_16_bold,0,0,LV_COORD_MAX,0);CHECK(size.x<=49);
    /* 100%省略小数后，以及最宽的非整数百分比，都必须放得进环内40px。 */
    lv_text_get_size(&size,"100%",&ui_font_12,0,0,LV_COORD_MAX,0);CHECK(size.x<=36);
    lv_text_get_size(&size,"99.9%",&ui_font_12,0,0,LV_COORD_MAX,0);CHECK(size.x<=40);
}
/* 同一时刻观察真实渲染后的Wi-Fi区域：连接/未连接必须不同，分隔线位置固定。
 * 只改变软件后端状态，不连接真实网络；离线必须灰色，联网才允许蓝色。 */
static void wifi_visual_contract(void)
{
    uint16_t disconnected[24*24];lv_display_t *display=lv_display_get_default();
    lv_obj_invalidate(page.header);lv_refr_now(display);
    for(unsigned y=0;y<24;++y)memcpy(disconnected+y*24,frame+(y+3)*480+384,24*2);
    uint16_t separator=frame[15*480+413];
    /* 末级抖色会按绝对坐标选取邻档，检查分隔线存在且网络状态变化不改变它。 */
    CHECK(separator!=frame[15*480+412]);
    dashboard_network_status(&ui,true,"Test","192.0.2.1");lv_refr_now(display);
    unsigned different=0,blue_on=0,blue_off=0;
    for(unsigned y=0;y<24;++y)for(unsigned x=0;x<24;++x) {
        uint16_t on=frame[(y+3)*480+x+384],off=disconnected[y*24+x];different+=(on!=off);
        blue_on+=((on&31)>((on>>11)&31)+5U);blue_off+=((off&31)>((off>>11)&31)+5U);
    }
    CHECK(different>5&&blue_on>10&&blue_off==0&&frame[15*480+413]==separator);
    screenshot("wifi_connected.ppm");dashboard_network_status(&ui,false,NULL,NULL);
    screenshot("wifi_disconnected.ppm");
}
/* 验证真实网络快照独立于模拟测量；UART在线但Wi-Fi离线是有效且常见的状态。
 * 直接检查RGB565渲染像素，防止只改布尔变量却漏掉头部失效/颜色。 */
static void camera_visual_contract(void)
{
    lv_display_t *display=lv_display_get_default();
    CHECK(page.session.simulated&&page.session.connected&&!page.camera_online);
    CameraNetworkState state={.online=1,.wifi=CAMERA_WIFI_OFFLINE};
    dashboard_network_update(&ui,&state);lv_refr_now(display);
    CHECK(page.camera_online&&!page.wifi_connected);
    unsigned green=0;
    for(unsigned y=3;y<28;++y)for(unsigned x=292;x<316;++x) {
        uint16_t color=frame[y*480+x];
        green+=(((color>>5)&63)/2U>((color>>11)&31)+5U);
    }
    CHECK(green>10);screenshot("camera_online_wifi_offline.ppm");
    state.wifi=CAMERA_WIFI_CONNECTED;
    dashboard_network_update(&ui,&state);lv_refr_now(display);
    CHECK(page.camera_online&&page.wifi_connected);screenshot("camera_wifi_online.ppm");
    memset(&state,0,sizeof(state));dashboard_network_update(&ui,&state);lv_refr_now(display);
    CHECK(!page.camera_online&&!page.wifi_connected&&page.session.simulated&&page.session.connected);
    green=0;
    for(unsigned y=3;y<28;++y)for(unsigned x=292;x<316;++x) {
        uint16_t color=frame[y*480+x];
        green+=(((color>>5)&63)/2U>((color>>11)&31)+5U);
    }
    CHECK(green==0);screenshot("camera_offline.ppm");
    lv_point_t size;
    lv_text_get_size(&size,"相机未连接",&ui_font_12,0,0,64,0);
    CHECK(size.x<=64&&size.y==ui_font_12.line_height);
    lv_text_get_size(&size,"相机已连接",&ui_font_12,0,0,64,0);
    CHECK(size.x<=64&&size.y==ui_font_12.line_height);
    puts("PASS: real UART status independent of demo; green/grey camera and blue/grey Wi-Fi; single-line header");
}
int main(void)
{
    lv_init();lv_tick_set_cb(HAL_GetTick);CHECK(GUI_DisplayInit());CHECK(GUI_RegisterTouch(reader,NULL));font_check();color_contract();gradient_contract();
    frame_stats_contract();
    instrument_defaults(&model);GUI_ApplyFixedCamera(&model);
    MeasurementResult r={.geometry=MEASUREMENT_RING,.status=MEASUREMENT_PENDING,.connected=true,.calibrated=true,.simulated=true};
    measurement_page_create(&page,lv_screen_active(),&r);GUI_DemoInit(&page,&model);dashboard_create(&ui,&page,&model,NULL);advance(60);
    lv_obj_add_event_cb(page.geometry.object,observe_overlap,LV_EVENT_DRAW_MAIN,NULL);
    wifi_visual_contract();
    camera_visual_contract();
    CHECK(page.selected_navigation==0&&!page.quality_mode&&instrument_model_check(&model));
    screenshot("measurement_idle.ppm");
    plot_dither_contract();
    footer_contract();
    progress_gradient_contract();
    /* 三个底部键可见区域以下一直到Y319，都必须命中原键，且相邻键不重叠。 */
    lv_obj_t *buttons[]={page.navigation[0],page.start_button,page.navigation[3]};
    int centers[]={81,240,399};
    for(unsigned i=0;i<3;++i)for(int y=310;y<=319;++y){lv_point_t p={centers[i],y};CHECK(lv_indev_search_obj(page.root,&p)==buttons[i]);}
    tap_at(page.start_button,240,319);incremental_contract("acquiring");
    advance(500);incremental_contract("progress/phase");advance(1500);
    CHECK(page.session.state==MEASUREMENT_COMPLETE&&model.record_count==1);
    CHECK(model.records[0].result.primary_cmm==3012&&!model.records[0].inspection);
    screenshot("measurement.ppm");
    redraw_contract();
    uint32_t scale=page.geometry.scale_q8;tap(page.zoom[0]);CHECK(page.geometry.scale_q8>scale);
    tap(page.zoom[1]);CHECK(page.geometry.scale_q8<4096);tap(page.zoom[2]);CHECK(page.geometry.scale_q8==GEOMETRY_DEFAULT_SCALE_Q8);
    tap_at(page.navigation[0],81,319);CHECK(page.quality_mode&&page.selected_navigation==1);
    screenshot("inspection_idle.ppm");plot_dither_contract();redraw_contract(); /* 两种布局的渐变/图标都核对条带边界。 */
    tap_at(page.start_button,240,319);CHECK(page.automatic);advance(2000);
    CHECK(page.session.state==MEASUREMENT_COMPLETE&&model.inspected==1);
    uint32_t completed_at=model.records[0].uptime_ms;
    CHECK(ui.next_auto_ms-completed_at==2500U);
    uint16_t *saved_result=malloc(174U*39U*2U);CHECK(saved_result);
    retain_result_contract(saved_result,true);
    screenshot("inspection_pass.ppm");
    /* 2.5秒截止前仍应是完成态；下一轮采集时结果卡片逐像素保持，随后才增加统计。 */
    advance(2500U-(now-completed_at)-5U);
    CHECK(page.session.state==MEASUREMENT_COMPLETE&&model.inspected==1);
    advance(150);CHECK(measurement_session_busy(&page.session)&&model.inspected==1);
    retain_result_contract(saved_result,false);free(saved_result);
    screenshot("inspection_acquiring.ppm");advance(1900);
    CHECK(model.inspected==2&&model.passed==model.inspected&&page.total_count==model.inspected);
    tap_at(page.start_button,240,319);CHECK(!page.automatic);
    /* 下一次改变标准后的失败要记到历史；旧记录仍然合格。 */
    model.settings.tolerance[0][0].target_cmm=2900;
    CHECK(measurement_page_start(&page));advance(2000);
    CHECK(model.records[0].verdict==INSPECTION_PRIMARY&&model.failed==1);
    CHECK(model.records[1].verdict==INSPECTION_PASS);
    screenshot("inspection.ppm");
    tap(page.navigation[2]);CHECK(ui.menu==DASH_RECORDS&&!lv_obj_is_hidden(ui.panel));screenshot("records.ppm");
    tap(ui.back);CHECK(lv_obj_is_hidden(ui.panel)&&page.quality_mode);
    tap(page.standard_button);CHECK(ui.menu==DASH_STANDARDS);
    tap(ui.rows[0]);CHECK(ui.edit);
    screenshot("numeric_keyboard.ppm");
    /* 对比底行五键主体与Y319输入结果/光标位置，验证移植后仍保留原补偿。 */
    lv_obj_update_layout(lv_screen_active());
    const lv_buttonmatrix_t *matrix=(const lv_buttonmatrix_t *)ui.keyboard;
    lv_area_t keys;lv_obj_get_coords(ui.keyboard,&keys);
    for(unsigned i=matrix->btn_cnt-5;i<matrix->btn_cnt;++i) {
        int x=keys.x1+(matrix->button_areas[i].x1+matrix->button_areas[i].x2)/2;
        int y=keys.y1+(matrix->button_areas[i].y1+matrix->button_areas[i].y2)/2;
        lv_textarea_set_text(ui.textarea,"12");lv_textarea_set_cursor_pos(ui.textarea,1);
        tap_at(ui.keyboard,x,y);
        char expected[32];snprintf(expected,sizeof(expected),"%s",lv_textarea_get_text(ui.textarea));
        int expected_cursor=lv_textarea_get_cursor_pos(ui.textarea);
        lv_textarea_set_text(ui.textarea,"12");lv_textarea_set_cursor_pos(ui.textarea,1);
        tap_at(ui.keyboard,x,319);
        CHECK(!strcmp(expected,lv_textarea_get_text(ui.textarea))&&expected_cursor==(int)lv_textarea_get_cursor_pos(ui.textarea));
    }
    lv_textarea_set_text(ui.textarea,"30.00");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);advance(50);
    CHECK(!ui.edit&&model.settings.tolerance[0][0].target_cmm==3000);
    /* 保存失败回滚整个设置，编辑器保留，不能显示伪成功。 */
    ui.services.save=reject_save;tap(ui.rows[0]);lv_textarea_set_text(ui.textarea,"31.00");
    lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);CHECK(ui.edit&&model.settings.tolerance[0][0].target_cmm==3000);
    lv_obj_send_event(ui.keyboard,LV_EVENT_CANCEL,NULL);ui.services.save=NULL;
    screenshot("standards.ppm");tap(ui.back);
    tap_at(page.navigation[3],399,319);CHECK(ui.menu==DASH_SETTINGS);screenshot("settings.ppm");
    /* 实际手指上滑：由输入系统驱动容器滚动，不只调用scroll_to伪装成触摸验证。 */
    tx=250;ty=245;down=true;advance(40);
    for(int y=245;y>=105;y-=10){ty=y;advance(20);}down=false;advance(200);
    CHECK(lv_obj_get_scroll_y(ui.body)>0);
    dashboard_open(&ui,DASH_NETWORK);advance(250);tap(ui.rows[0]);CHECK(ui.edit);
    lv_textarea_set_text(ui.textarea,"Lab_WiFi");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    screenshot("wifi_keyboard.ppm");
    lv_textarea_set_text(ui.textarea,"demo12345");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    CHECK(!ui.connected&&!ui.connecting&&strstr(ui.message,"连接失败"));
    DashboardNetworkOps net={.connect=connect_request,.disconnect=disconnect_request};dashboard_set_network(&ui,&net);
    tap(ui.rows[0]);lv_textarea_set_text(ui.textarea,"Lab_WiFi");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    lv_textarea_set_text(ui.textarea,"demo12345");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    CHECK(connects==1&&ui.connecting&&!ui.connected&&!ui.password[0]);
    dashboard_network_status(&ui,true,"Lab_WiFi","192.168.1.6");CHECK(ui.connected&&page.wifi_connected);
    screenshot("wifi.ppm");tap(ui.rows[1]);CHECK(disconnects==1&&ui.connected);
    dashboard_network_status(&ui,false,NULL,NULL);CHECK(!ui.connected);
    wifi_scan_contract();
    dashboard_open(&ui,DASH_DEVICE);advance(250);screenshot("device.ppm");
    /* 公差边界两端都合格，超出一单位失败；无效测量不能合格。 */
    InstrumentSettings s=model.settings;MeasurementResult q={.geometry=MEASUREMENT_RING,.primary_cmm=2980,.secondary_cmm=1190,.status=MEASUREMENT_VALID};
    CHECK(instrument_inspect(&s,&q)==INSPECTION_PASS);q.primary_cmm=3020;q.secondary_cmm=1210;CHECK(instrument_inspect(&s,&q)==INSPECTION_PASS);
    ++q.primary_cmm;CHECK(instrument_inspect(&s,&q)==INSPECTION_PRIMARY);q.primary_cmm=3000;++q.secondary_cmm;CHECK(instrument_inspect(&s,&q)==INSPECTION_SECONDARY);
    q.status=MEASUREMENT_INVALID;CHECK(instrument_inspect(&s,&q)==INSPECTION_INVALID);
    dashboard_close(&ui);advance(250);measurement_page_navigate(&page,0);
    model.settings.mode=MEASUREMENT_RECTANGLE;measurement_page_select_geometry(&page,MEASUREMENT_RECTANGLE);
    CHECK(measurement_page_start(&page));advance(2000);CHECK(page.session.result.primary_cmm==3200&&page.session.result.secondary_cmm==2000);
    screenshot("rectangle.ppm");redraw_contract();
    /* 超过容量后只保留64条，累计统计不因滚动淘汰而减少。 */
    for(unsigned i=0;i<70;++i)CHECK(instrument_add_record(&model,&page.session.result));
    CHECK(model.record_count==64&&instrument_model_check(&model));
    dashboard_open(&ui,DASH_RECORDS);advance(250);CHECK(lv_obj_get_scroll_bottom(ui.body)>0);
    /* 手指拖动滚动记录列表，检查末尾有入口继续加载下一段。 */
    tx=260;ty=245;down=true;advance(40);
    for(int y=245;y>=90;y-=10){ty=y;advance(20);}down=false;advance(200);CHECK(lv_obj_get_scroll_y(ui.body)>0);
    screenshot("records_scroll.ppm");dashboard_close(&ui);advance(250);
    /* 重复打开不同二级页，检查销毁/重建不会耗尽当前配置的LVGL池。 */
    for(unsigned i=0;i<30;++i){
        dashboard_open(&ui,(DashboardMenu)(i%7));advance(200);dashboard_close(&ui);
        /* 每种菜单均通过同一返回入口，关闭后不残留动画或输入遮挡。 */
        CHECK(lv_obj_is_hidden(ui.panel)&&!ui.animating&&!page.presentation_paused);
        CHECK(lv_anim_get(ui.panel,NULL)==NULL&&!lv_obj_is_clickable(ui.transition_guard));
        advance(200);CHECK(lv_mem_test()==LV_RESULT_OK);
    }
    MeasurementBackend original=page.backend,quiet={.start=silent_backend};measurement_page_set_backend(&page,&quiet);
    measurement_page_navigate(&page,1);page.session.timeout_ms=2000;uint32_t count=model.inspected;
    dashboard_action(&ui);CHECK(page.automatic);advance(2300);
    CHECK(page.session.state==MEASUREMENT_FAILED&&!page.automatic&&model.inspected==count);
    measurement_page_set_backend(&page,&original);
    dirty_area_contract();
    animation_contract();
    reopen_transfer_contract();
    strip_comparison_contract();
    record_cache_update_contract();
    occlusion_contract();
    presentation_contract();
    menu_reuse_contract();
    update_entry_contract();
    padded_flush_contract();
    asynchronous_contract();
    lv_mem_monitor_t monitor;lv_mem_monitor(&monitor);
    CHECK(!in_flight&&dma_starts==dma_finishes&&draw_while_dma>0);
    printf("PASS: deferred DMA ownership/tail drain; %u blocks, %u overlapping draw callbacks, %llu bytes\n",
           dma_starts,draw_while_dma,(unsigned long long)sent_bytes);
    printf("PASS: modes, bottom touch, auto inspection, tolerance, records, scroll, rollback, Wi-Fi contracts, rectangle; heap free=%lu max_used=%lu\n",(unsigned long)monitor.free_size,(unsigned long)monitor.max_used);
    dashboard_destroy(&ui);return 0;
}
