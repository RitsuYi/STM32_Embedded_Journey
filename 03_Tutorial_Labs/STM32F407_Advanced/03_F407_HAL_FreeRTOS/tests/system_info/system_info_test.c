#include "instrument/dashboard_ui.h"
#include "gui_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}}while(0)
/* 真实LVGL负责点击、拖动、裁剪和52KiB池分配。仅任务快照和显示硬件为替身。 */
static uint32_t ms;static uint8_t buffer[480U*32U*2U],frame[480U*320U*2U];
static int32_t touch_x,touch_y;static bool down;
static MeasurementPage page;static InstrumentModel model;static DashboardUI ui;
static SystemMonitorSnapshot sample;
static unsigned sample_reads;
static uint32_t now(void){return ms;}
void GUI_LVGL_AssertFailed(const char *file,int line)
{fprintf(stderr,"ASSERT %s:%d\n",file,line);exit(1);}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *pixels)
{
    lv_draw_buf_t *active=lv_display_get_buf_active(d);unsigned width=(unsigned)lv_area_get_width(a);
    for(int y=a->y1;y<=a->y2;++y)memcpy(frame+((unsigned)y*480U+(unsigned)a->x1)*2U,
        pixels+(unsigned)(y-a->y1)*active->header.stride,width*2U);
    lv_display_flush_ready(d);
}
static void input(lv_indev_t *i,lv_indev_data_t *d)
{(void)i;d->point=(lv_point_t){touch_x,touch_y};d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static const SystemMonitorSnapshot *read_sample(void *ctx)
{(void)ctx;++sample_reads;sample.uptime_ms=2772000U;return &sample;}
static void advance(unsigned duration)
{
    for(unsigned i=0;i<duration;i+=5) {ms+=5;lv_timer_handler();CHECK(lv_mem_test()==LV_RESULT_OK);}
}
static void tap(lv_obj_t *o)
{
    lv_obj_update_layout(lv_screen_active());lv_area_t a;lv_obj_get_coords(o,&a);
    touch_x=(a.x1+a.x2)/2;touch_y=(a.y1+a.y2)/2;down=true;advance(60);down=false;advance(100);
}
static void save(const char *name)
{
    /* 仅截图用同一组已确认预览数据，便于直接核对布局；不伪称这些是板上读数。
     * 分配器本身仍运行真实52KiB池，下面只是替换绘图快照，不改变任何真实分配。 */
    ui.system.memory.free_size=17613U;ui.system.memory.free_biggest_size=14848U;
    ui.system.memory.max_used=42291U;page.clock_seconds=2772U;
    lv_obj_invalidate(page.header);if(ui.system.content)lv_obj_invalidate(ui.system.content);lv_refr_now(lv_display_get_default());
    FILE *f=fopen(name,"wb");CHECK(f);fprintf(f,"P6\n480 320\n255\n");
    for(unsigned i=0;i<480U*320U;++i){uint16_t p=frame[2*i]|(uint16_t)frame[2*i+1]<<8;
        fputc(((p>>11)&31U)*255U/31U,f);fputc(((p>>5)&63U)*255U/63U,f);fputc((p&31U)*255U/31U,f);}
    fclose(f);
}
int main(void)
{
    lv_init();lv_tick_set_cb(now);lv_display_t *d=lv_display_create(480,320);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);lv_indev_t *pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer,input);lv_timer_set_period(lv_indev_get_read_timer(pointer),15);
    instrument_defaults(&model);MeasurementResult initial={.geometry=MEASUREMENT_RECTANGLE,.status=MEASUREMENT_PENDING,.simulated=true};
    measurement_page_create(&page,lv_screen_active(),&initial);GUI_DemoInit(&page,&model);
    dashboard_create(&ui,&page,&model,NULL);ui.system.read=read_sample;
    sample=(SystemMonitorSnapshot){.available=true,.complete=true,.cpu_valid=true,.scheduler_running=true,.cpu_permille=368,.count=6,.total_tasks=6,
        .heap_bytes=24576,.heap_free=10036,.min_heap_free=9523,.largest_heap_block=8909,
        .ram_reserved=106192,.ccm_reserved=53248,.flash_used=750160};
    const char *names[]={"GUI","Camera","OLED","Buzzer","Tmr Svc","IDLE"};
    const uint32_t allocated[]={8192,2048,1024,512,512,512},used[]={2150,666,389,123,205,102};
    const uint32_t minimum[]={3328,963,317,287,195,328};
    const uint16_t cpu[]={324,28,11,1,4,632};
    for(unsigned i=0;i<6;++i){SystemTaskInfo *t=&sample.tasks[i];t->id=i+1U;
        snprintf(t->name,sizeof(t->name),"%s",names[i]);t->stack_bytes=allocated[i];t->used_bytes=used[i];
        t->min_free_bytes=minimum[i];t->cpu_valid=true;t->current_valid=true;t->cpu_permille=cpu[i];}
    advance(100);dashboard_open(&ui,DASH_SETTINGS);advance(250);save("system-settings.ppm");
    tap(ui.rows[1]);advance(250);CHECK(ui.menu==DASH_DEVICE&&ui.system.root);
    CHECK(!strcmp(lv_label_get_text(ui.title),"系统信息"));save("system-overview.ppm");
    sample.scheduler_running=false;system_info_refresh(&ui.system);advance(30);save("system-scheduler-paused.ppm");
    sample.scheduler_running=true;
    /* 使用真实100ms页面定时器检查1秒刷新，不把顶部时钟跳字当作数据刷新。 */
    system_info_refresh(&ui.system);unsigned read_before=sample_reads;
    advance(900);CHECK(sample_reads==read_before);
    advance(200);CHECK(sample_reads==read_before+1U);
    advance(1000);CHECK(sample_reads==read_before+2U);
    sample.cpu_permille=0U;system_info_refresh(&ui.system);advance(30);save("system-cpu-zero.ppm");
    sample.cpu_permille=1000U;system_info_refresh(&ui.system);advance(30);save("system-cpu-full.ppm");
    sample.cpu_permille=368U;
    sample.cpu_valid=false;system_info_refresh(&ui.system);advance(30);save("system-waiting.ppm");
    sample.cpu_valid=true;tap(ui.system.tabs[1]);CHECK(ui.system.list&&ui.system.row_count==6);
    save("system-tasks.ppm");lv_obj_t *list=ui.system.list;
    /* 六个真实任务均应落在可见正文中；表格下边贴近正文底部，不再留旧版空白。 */
    lv_area_t list_area,last_row,content_area;
    lv_obj_get_coords(list,&list_area);lv_obj_get_coords(ui.system.rows[5],&last_row);
    lv_obj_get_coords(ui.system.content,&content_area);
    CHECK(last_row.y2<=list_area.y2+1&&content_area.y2-list_area.y2==1);
    /* 扩展为8个任务再验证滚动，避免把六任务一屏显示误当成失去拖动能力。 */
    for(unsigned i=6;i<8;++i) {sample.tasks[i]=sample.tasks[5];sample.tasks[i].id=i+1U;
        snprintf(sample.tasks[i].name,sizeof(sample.tasks[i].name),"Worker%u",i);}
    sample.count=8;sample.total_tasks=8;system_info_refresh(&ui.system);advance(30);
    lv_area_t before;lv_obj_get_coords(ui.system.tabs[1],&before);
    /* 不伪造CLICKED：输入在真实列表上向上拖动，确认只滚动，不误进任务详情。 */
    touch_x=250;touch_y=245;down=true;advance(40);
    for(unsigned i=0;i<9;++i){touch_y-=10;advance(25);}down=false;advance(250);
    /* 松手后列表仍可能因惯性继续滑动，先等待真实滚动结束，再检查每秒刷新是否保留位置。
     * 最多等1.5秒；若始终不停则报错，不能把正常惯性位移误判为刷新重置。 */
    for(unsigned waited=0;waited<1500U&&lv_obj_is_scrolling(list);waited+=50U)advance(50);
    CHECK(!lv_obj_is_scrolling(list));
    CHECK(lv_obj_get_scroll_y(list)>0&&!ui.system.selected);
    lv_area_t after;lv_obj_get_coords(ui.system.tabs[1],&after);CHECK(before.y1==after.y1);
    /* 开启边缘弹性后，松手250ms时可能仍在回位；等原生惯性结束再核刷新保位。 */
    advance(1000);int32_t scroll=lv_obj_get_scroll_y(list);advance(1200);
    CHECK(ui.system.list==list&&lv_obj_get_scroll_y(list)==scroll);save("system-tasks-scrolled.ppm");
    lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);advance(30);tap(ui.system.rows[0]);CHECK(ui.system.selected==1U);
    save("system-task-detail.ppm");tap(ui.back);CHECK(!ui.system.selected&&ui.menu==DASH_DEVICE);
    sample.count=6;sample.total_tasks=6;system_info_refresh(&ui.system);
    tap(ui.system.tabs[2]);save("system-memory.ppm");tap(ui.system.tabs[3]);save("system-device.ppm");
    CHECK(lv_obj_get_scroll_bottom(ui.system.content)>0);
    lv_obj_scroll_to_y(ui.system.content,1000,LV_ANIM_OFF);advance(30);save("system-device-bottom.ppm");
    sample.complete=false;sample.count=0;tap(ui.system.tabs[1]);advance(40);save("system-capacity.ppm");
    sample.complete=true;sample.count=6;system_info_refresh(&ui.system);
    sample.available=false;system_info_refresh(&ui.system);advance(40);save("system-unavailable.ppm");sample.available=true;
    /* 比较相同的最后一个标签，避免把任务表比设备页多几个控件误报成泄漏。 */
    /* 返回恢复直接关闭，确认没有在途退出动画，再比较相同页面的空闲内存。 */
    tap(ui.system.tabs[3]);dashboard_close(&ui);CHECK(lv_obj_is_hidden(ui.panel)&&!ui.animating);
    advance(100);lv_mem_monitor_t first;lv_mem_monitor(&first);
    for(unsigned i=0;i<20;++i){dashboard_open(&ui,DASH_DEVICE);advance(220);
        for(unsigned tab=0;tab<4;++tab)tap(ui.system.tabs[tab]);
        dashboard_close(&ui);advance(100);}
    lv_mem_monitor_t end;lv_mem_monitor(&end);
    printf("Memory: before=%lu after=%lu peak=%lu\n",(unsigned long)first.free_size,
        (unsigned long)end.free_size,(unsigned long)end.max_used);
    CHECK(end.free_size>=first.free_size&&end.max_used<52U*1024U-2048U);
    printf("PASS: real touch/drag, fixed header, stable scroll, details/back, 4 tabs, unavailable/capacity, 20 reopen; peak=%lu free=%lu biggest=%lu\n",
        (unsigned long)end.max_used,(unsigned long)end.free_size,(unsigned long)end.free_biggest_size);
    dashboard_destroy(&ui);lv_deinit();return 0;
}
