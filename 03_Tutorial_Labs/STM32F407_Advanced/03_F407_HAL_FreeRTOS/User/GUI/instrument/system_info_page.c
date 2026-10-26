#include "system_info_page.h"
#include "../measurement/ui_draw.h"
#include "../fonts/measurement_fonts.h"
#include "../gui_version.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* 本页采用小标题12px、标签13px、数字18/24px，不能直接用原主页14/26px粗体代替。
 * 专用字体仅包含本页文案，字形放Flash；原主页字体和LVGL核心保持不变。 */
LV_FONT_DECLARE(system_font_11);
LV_FONT_DECLARE(system_font_12);
LV_FONT_DECLARE(system_font_12_bold);
LV_FONT_DECLARE(system_font_13);
LV_FONT_DECLARE(system_font_13_bold);
LV_FONT_DECLARE(system_font_16_bold);
LV_FONT_DECLARE(system_font_18);
LV_FONT_DECLARE(system_font_22);
LV_FONT_DECLARE(system_font_24);

/* 自绘卡片/行仅生成当次条带需要的图元，不使用整屏图片或额外像素缓冲。
 * 坐标相对466×246菜单正文；上方原有返回栏和全局状态栏保持不变。 */
static void build_content(SystemInfoPage *p);
/* 统一正文的8px外边距/卡片间距，左右卡片均为221px。
 * 标签占38px，正文剩208px；任务表用28px表头+6行30px填满正文，避免底部大块留白。
 * 六个任务全部可见，未来新增任务仍使用原生上下滚动，不压缩字号或触摸行高。 */
enum { CONTENT_W=466,CONTENT_H=208,INSET=8,GAP=8,CARD_W=221,
       TABLE_HEADER=28,TASK_ROW=30 };
static lv_obj_t *area(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_scrollable(o,false);
    return o;
}
static void kib(char *out,size_t capacity,uint32_t bytes)
{
    /* 只用整数格式化KiB，避免把浮点格式化的额外栈消耗带进每个任务行。 */
    uint32_t hundredths=(uint32_t)(((uint64_t)bytes*100U+512U)/1024U);
    snprintf(out,capacity,"%" PRIu32 ".%02" PRIu32,hundredths/100U,hundredths%100U);
}
static void kib_one(char *out,size_t capacity,uint32_t bytes)
{
    uint32_t tenths=(uint32_t)(((uint64_t)bytes*10U+512U)/1024U);
    snprintf(out,capacity,"%" PRIu32 ".%" PRIu32,tenths/10U,tenths%10U);
}
static void percent(char *out,size_t capacity,uint16_t value,bool valid)
{
    if(valid)snprintf(out,capacity,"%u.%u%%",value/10U,value%10U);
    else snprintf(out,capacity,"--");
}
static void text(lv_layer_t *l,int x,int y,int w,const char *s,bool bold)
{ui_text(l,x,y,w,bold?&system_font_12_bold:&system_font_12,UI_INK,s,LV_TEXT_ALIGN_LEFT);}
static void small(lv_layer_t *l,int x,int y,int w,const char *s,lv_text_align_t align)
{ui_text(l,x,y,w,&system_font_11,UI_MUTED,s,align);}
static int number(lv_layer_t *l,int x,int baseline,int w,const char *s,const lv_font_t *font)
{
    ui_text(l,x,baseline-font->line_height+font->base_line,w,font,UI_INK,s,LV_TEXT_ALIGN_LEFT);
    lv_point_t size;lv_text_get_size(&size,s,font,0,0,LV_COORD_MAX,0);return size.x;
}
static void card(lv_layer_t *l,int x,int y,int w,int h)
{ui_rect(l,x,y,w,h,UI_PANEL,255,8,UI_BORDER,1);}
static void bar(lv_layer_t *l,int x,int y,int w,uint32_t used,uint32_t total,uint32_t color)
{
    ui_rect(l,x,y,w,5,0xEDF0F3,255,2,0,0);
    if(total) {if(used>total)used=total;int n=(int)((uint64_t)used*(unsigned)w/total);
        if(n)ui_rect(l,x,y,n,5,color,255,2,0,0);}
}
static void scheduler(lv_layer_t *l,int x,int y,bool running)
{
    /* 用矢量图元画“运行/暂停”标记，不增加位图或图标字体。
     * y是图标中心，与左侧CPU蓝条的中心对齐，状态取真实调度器快照。 */
    uint32_t color=running?0x62B995:UI_MUTED;
    ui_rect(l,x,y-8,16,16,running?0xE6F6EE:0xEDF0F3,255,8,0,0);
    if(running) {
        ui_line(l,x+4,y,x+7,y+3,color,2);
        ui_line(l,x+7,y+3,x+12,y-3,color,2);
    } else {
        ui_line(l,x+6,y-3,x+6,y+3,color,2);
        ui_line(l,x+10,y-3,x+10,y+3,color,2);
    }
    ui_text(l,x+23,y-9,175,&system_font_12,color,
            running?"调度器运行中":"调度器未运行",LV_TEXT_ALIGN_LEFT);
}
static const SystemTaskInfo *selected_task(SystemInfoPage *p)
{
    if(p->snapshot)for(unsigned i=0;i<p->snapshot->count;++i)
        if(p->snapshot->tasks[i].id==p->selected)return &p->snapshot->tasks[i];
    return NULL;
}
static void draw_row(lv_event_t *event)
{
    SystemInfoPage *p=lv_event_get_user_data(event);lv_obj_t *o=lv_event_get_target_obj(event);
    unsigned i=(unsigned)(uintptr_t)lv_obj_get_user_data(o);
    if(!p->snapshot || i>=p->snapshot->count)return;
    const SystemTaskInfo *t=&p->snapshot->tasks[i];lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *l=lv_event_get_layer(event);int x=a.x1,y=a.y1;char s[48],v[20],w[20];
    ui_rect(l,x,y,448,TASK_ROW,lv_obj_has_state(o,LV_STATE_PRESSED)?0xE8F3FD:UI_WHITE,255,0,0,0);
    if(i+1U<p->snapshot->count)ui_line(l,x,y+TASK_ROW-1,x+447,y+TASK_ROW-1,0xEDF0F3,1);
    text(l,x+9,y+7,64,t->name,false);percent(s,sizeof(s),t->cpu_permille,t->cpu_valid);
    ui_text(l,x+85,y+7,49,&system_font_12,UI_INK,s,LV_TEXT_ALIGN_RIGHT);
    kib(v,sizeof(v),t->used_bytes);kib_one(w,sizeof(w),t->stack_bytes);
    if(t->current_valid)snprintf(s,sizeof(s),"≈%s / %s",v,w);
    else snprintf(s,sizeof(s),"-- / %s",w);
    ui_text(l,x+152,y+7,148,&system_font_12,UI_INK,s,LV_TEXT_ALIGN_RIGHT);
    kib(s,sizeof(s),t->min_free_bytes);
    ui_text(l,x+318,y+7,121,&system_font_12,UI_INK,s,LV_TEXT_ALIGN_RIGHT);
}
static void draw_content(lv_event_t *event)
{
    SystemInfoPage *p=lv_event_get_user_data(event);lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(event),&a);int x=a.x1,y=a.y1;
    lv_layer_t *l=lv_event_get_layer(event);char s[96],v[24];
    const SystemMonitorSnapshot *m=p->snapshot;
    if((!m||!m->available)&&p->tab!=3U) {text(l,x+16,y+16,430,"统计数据暂不可用",false);return;}
    if(p->tab==0U) {
        /* 上层两张等宽等高卡片，下层一张通栏卡片，总高恰好填满208px正文。 */
        card(l,x+INSET,y,CARD_W,110);card(l,x+INSET+CARD_W+GAP,y,CARD_W,110);
        text(l,x+20,y+11,198,"CPU 占用率",true);
        if(m->cpu_valid) {
            snprintf(s,sizeof(s),"%u.%u",m->cpu_permille/10U,m->cpu_permille%10U);
            int width=number(l,x+20,y+57,198,s,&system_font_24);
            small(l,x+23+width,y+44,22,"%",LV_TEXT_ALIGN_LEFT);
        } else number(l,x+20,y+57,198,"--",&system_font_24);
        bar(l,x+20,y+72,198,m->cpu_valid?m->cpu_permille:0U,1000U,UI_CYAN);
        small(l,x+20,y+88,198,"STM32F407VGT6 · 168 MHz",LV_TEXT_ALIGN_LEFT);
        text(l,x+249,y+11,198,"FreeRTOS 任务数",true);
        snprintf(s,sizeof(s),"%u",m->total_tasks);
        int width=number(l,x+249,y+57,198,s,&system_font_24);
        small(l,x+254+width,y+44,30,"个",LV_TEXT_ALIGN_LEFT);
        scheduler(l,x+249,y+74,m->scheduler_running);
        small(l,x+249,y+88,198,"FreeRTOS 11.1.0",LV_TEXT_ALIGN_LEFT);
        card(l,x+8,y+118,450,90);text(l,x+20,y+129,426,"动态内存 · 当前已用 / 容量",true);
        const char *n[]={"FreeRTOS","LVGL"};
        /* 显示配置的整池容量，已用量包含分配器管理开销；free_size才是真正可用余量。
         * lv_mem_monitor.total_size只统计块内空间，会排除管理区，不能把它当52KiB总预留。 */
        uint32_t used[]={m->heap_bytes-m->heap_free,(uint32_t)(LV_MEM_SIZE-p->memory.free_size)};
        uint32_t total[]={m->heap_bytes,LV_MEM_SIZE};
        for(unsigned i=0;i<2;++i) {
            int yy=y+154+(int)i*26;text(l,x+20,yy,82,n[i],false);
            bar(l,x+110,yy+6,222,used[i],total[i],i?UI_PURPLE:UI_CYAN);
            kib_one(v,sizeof(v),used[i]);snprintf(s,sizeof(s),"%s / %" PRIu32 " KiB",v,total[i]/1024U);
            ui_text(l,x+339,yy,106,&system_font_12,UI_MUTED,s,LV_TEXT_ALIGN_RIGHT);
        }
    } else if(p->tab==1U) {
        if(p->selected) {
            const SystemTaskInfo *t=selected_task(p);
            if(!t){text(l,x+16,y+16,430,"任务已退出，请返回列表",false);return;}
            ui_text(l,x+8,y+8,200,&system_font_16_bold,UI_INK,t->name,LV_TEXT_ALIGN_LEFT);
            const char *states[]={"运行","就绪","阻塞","挂起","已删除","未知"};
            snprintf(s,sizeof(s),"优先级 %u · %s",t->priority,states[t->state<5U?t->state:5U]);
            small(l,x+220,y+10,238,s,LV_TEXT_ALIGN_RIGHT);
            card(l,x+8,y+38,221,96);card(l,x+237,y+38,221,96);
            text(l,x+20,y+49,198,"当前已用栈 · 快照估算",true);kib(v,sizeof(v),t->used_bytes);
            snprintf(s,sizeof(s),t->current_valid?"≈%s":"--",v);
            int width=number(l,x+20,y+95,198,s,&system_font_22);
            small(l,x+23+width,y+80,60,"KiB",LV_TEXT_ALIGN_LEFT);
            text(l,x+249,y+49,198,"历史最小剩余栈",true);kib(v,sizeof(v),t->min_free_bytes);
            width=number(l,x+249,y+95,198,v,&system_font_22);
            small(l,x+252+width,y+80,60,"KiB",LV_TEXT_ALIGN_LEFT);
            bar(l,x+20,y+115,198,t->used_bytes,t->stack_bytes,UI_CYAN);
            kib_one(v,sizeof(v),t->stack_bytes);snprintf(s,sizeof(s),"分配总量 %s KiB",v);
            small(l,x+249,y+111,198,s,LV_TEXT_ALIGN_LEFT);
            card(l,x+8,y+142,450,78);ui_line(l,x+20,y+181,x+445,y+181,UI_BORDER,1);
            text(l,x+20,y+154,260,"CPU 占比 · 每秒更新",false);
            percent(s,sizeof(s),t->cpu_permille,t->cpu_valid);
            ui_text(l,x+320,y+154,125,&system_font_12,UI_MUTED,s,LV_TEXT_ALIGN_RIGHT);
            text(l,x+20,y+193,260,"历史栈水位换算",false);
            kib(v,sizeof(v),t->stack_bytes>t->min_free_bytes?t->stack_bytes-t->min_free_bytes:0U);
            snprintf(s,sizeof(s),"约 %s KiB 已触及",v);
            ui_text(l,x+278,y+193,167,&system_font_12,UI_MUTED,s,LV_TEXT_ALIGN_RIGHT);
            small(l,x+8,y+230,450,"当前快照看变化，历史最小余量看栈是否够用。",LV_TEXT_ALIGN_LEFT);
        } else if(!m->complete)text(l,x+16,y+16,430,"任务数量超过统计容量",false);
        else {
            card(l,x+8,y,450,CONTENT_H);
            ui_rect(l,x+9,y+1,448,TABLE_HEADER-1,0xF3F6F9,255,7,0,0);
            ui_rect(l,x+9,y+20,448,8,0xF3F6F9,255,0,0,0);
            text(l,x+18,y+6,65,"任务",false);
            ui_text(l,x+94,y+6,49,&system_font_12,UI_MUTED,"CPU(1s)",LV_TEXT_ALIGN_RIGHT);
            small(l,x+161,y+7,148,"当前≈ / 分配栈 KiB",LV_TEXT_ALIGN_RIGHT);
            small(l,x+327,y+7,121,"历史最小余栈 KiB",LV_TEXT_ALIGN_RIGHT);
        }
    } else if(p->tab==2U) {
        uint32_t total[]={m->heap_bytes,LV_MEM_SIZE};
        uint32_t free_now[]={m->heap_free,(uint32_t)p->memory.free_size};
        uint32_t largest[]={m->largest_heap_block,(uint32_t)p->memory.free_biggest_size};
        const char *names[]={"FreeRTOS 堆","LVGL 内存池"};
        for(unsigned i=0;i<2;++i) {
            int xx=x+8+(int)i*229;card(l,xx,y,221,108);text(l,xx+12,y+11,197,names[i],true);
            kib_one(v,sizeof(v),total[i]-free_now[i]);
            int width=number(l,xx+11,y+46,199,v,&system_font_18);
            snprintf(s,sizeof(s),"/ %" PRIu32 " KiB 已用",total[i]/1024U);
            number(l,xx+14+width,y+46,196-width,s,&system_font_12);
            /* 保留原来的字体、文案、单位和左右两行，仅重新分配纵向留白。
             * 条上移2px，说明行分别下移4/6px，让文字间距舒展、底部不再空一大截。 */
            bar(l,xx+11,y+56,199,total[i]-free_now[i],total[i],i?UI_PURPLE:UI_CYAN);
            kib_one(v,sizeof(v),free_now[i]);snprintf(s,sizeof(s),"当前剩余 %sK",v);
            small(l,xx+11,y+69,199,s,LV_TEXT_ALIGN_LEFT);
            kib_one(v,sizeof(v),i?(uint32_t)p->memory.max_used:m->min_heap_free);
            snprintf(s,sizeof(s),"%s %sK",i?"峰值":"最低",v);
            small(l,xx+11,y+69,199,s,LV_TEXT_ALIGN_RIGHT);
            small(l,xx+11,y+88,199,"最大空闲块",LV_TEXT_ALIGN_LEFT);
            kib_one(v,sizeof(v),largest[i]);snprintf(s,sizeof(s),"%s KiB",v);
            small(l,xx+11,y+88,199,s,LV_TEXT_ALIGN_RIGHT);
        }
        card(l,x+8,y+116,450,92);
        text(l,x+20,y+125,426,"当前 ELF 链接布局 · 静态预留",true);
        const char *names2[]={"普通 SRAM","CCM","Flash"};
        uint32_t used[]={m->ram_reserved,m->ccm_reserved,m->flash_used};
        uint32_t totals[]={128U*1024U,64U*1024U,1024U*1024U};
        for(unsigned i=0;i<3;++i) {
            int yy=y+146+(int)i*20;small(l,x+20,yy,67,names2[i],LV_TEXT_ALIGN_LEFT);
            bar(l,x+94,yy+7,212,used[i],totals[i],i==1?UI_PURPLE:i==2?0xB5C6D6:UI_CYAN);
            kib_one(v,sizeof(v),used[i]);snprintf(s,sizeof(s),"%s / %" PRIu32 " KiB",v,totals[i]/1024U);
            small(l,x+314,yy,132,s,LV_TEXT_ALIGN_RIGHT);
        }
    } else {
        /* 设备原有信息全部保留在可滚动正文中，不新建第二个设置入口。 */
        const char *names[]={"设备名称","主控","固件版本","显示屏","触摸","图形库","实时内核","视觉前端","存储"};
        const char *values[]={"智能视觉测量仪","STM32F407VGT6 / 168MHz",GUI_FIRMWARE_VERSION,
            "ST7796 / 480 x 320 / RGB565","FT6336U / I2C2","LVGL 9.6.0","FreeRTOS 11.1.0 / TIM5 1MHz",
            "MaixCAM2",p->persistent?"后端已接入":"仅内存 / 重启后清空"};
        for(unsigned i=0;i<9U;++i) {
            int yy=y+(int)i*48;card(l,x+8,yy,450,40);text(l,x+20,yy+12,108,names[i],false);
            ui_text(l,x+130,yy+12,316,&system_font_12,UI_MUTED,values[i],LV_TEXT_ALIGN_RIGHT);
        }
    }
}
static void row_click(lv_event_t *event)
{
    SystemInfoPage *p=lv_event_get_user_data(event);
    unsigned i=(unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event));
    if(p->snapshot && i<p->snapshot->count) {p->selected=p->snapshot->tasks[i].id;build_content(p);}
}
static void update_rows(SystemInfoPage *p)
{
    if(!p->list||!p->snapshot)return;
    lv_obj_set_hidden(p->list,!p->snapshot->available||!p->snapshot->complete);
    unsigned count=p->snapshot->count;bool same=count==p->row_count;
    for(unsigned i=0;i<count&&same;++i)same=p->row_ids[i]==p->snapshot->tasks[i].id;
    if(!same) {
        /* 只有任务名单变化才重建，平常每秒只失效可见行，保留手指滚动位置。 */
        lv_obj_clean(p->list);p->row_count=count;
        for(unsigned i=0;i<count;++i) {
            p->row_ids[i]=p->snapshot->tasks[i].id;
            lv_obj_t *o=area(p->list,0,(int)i*TASK_ROW,448,TASK_ROW);p->rows[i]=o;
            lv_obj_set_clickable(o,true);lv_obj_set_user_data(o,(void *)(uintptr_t)i);
            lv_obj_add_event_cb(o,draw_row,LV_EVENT_DRAW_MAIN,p);
            lv_obj_add_event_cb(o,row_click,LV_EVENT_CLICKED,p);
        }
    }
    for(unsigned i=0;i<count;++i)lv_obj_invalidate(p->rows[i]);
}
static void build_content(SystemInfoPage *p)
{
    lv_obj_clean(p->content);p->list=NULL;p->row_count=0;
    bool detail=p->tab==1U&&p->selected;
    for(unsigned i=0;i<4U;++i)lv_obj_set_hidden(p->tabs[i],detail);
    lv_obj_set_y(p->content,detail?0:38);lv_obj_set_height(p->content,detail?246:CONTENT_H);
    if(p->title)lv_label_set_text(p->title,detail?"任务详情":"系统信息");
    lv_obj_set_scrollable(p->content,p->tab==3U);
    /* 所有绘制都在独立子对象上，切标签时可释放而不影响上面的四个按钮。 */
    lv_obj_t *canvas=area(p->content,0,0,CONTENT_W,p->tab==3U?424:detail?246:CONTENT_H);
    lv_obj_add_event_cb(canvas,draw_content,LV_EVENT_DRAW_MAIN,p);
    if(p->tab==1U&&!p->selected) {
        /* 底边留1px给卡片边框；只让行区域滚动，表头和标签始终固定。 */
        p->list=area(p->content,9,TABLE_HEADER,448,CONTENT_H-TABLE_HEADER-1);
        lv_obj_set_scrollable(p->list,true);lv_obj_set_scroll_dir(p->list,LV_DIR_VER);
        lv_obj_set_scrollbar_mode(p->list,LV_SCROLLBAR_MODE_AUTO);
        /* 与设置/历史同样使用原生惯性和双向边缘回弹；只移动任务行，表头固定。 */
        lv_obj_set_scroll_momentum(p->list,true);lv_obj_set_scroll_elastic(p->list,true);
        lv_obj_set_scroll_chain(p->list,false);update_rows(p);
        lv_obj_set_style_width(p->list,3,LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_color(p->list,ui_color(UI_BORDER),LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(p->list,LV_OPA_COVER,LV_PART_SCROLLBAR);
    }
    /* 设备长页沿用按手势速度计算的惯性与回弹；其他标签仍由scrollable禁止滚动。 */
    lv_obj_set_scroll_dir(p->content,LV_DIR_VER);lv_obj_set_scroll_momentum(p->content,true);
    lv_obj_set_scroll_elastic(p->content,true);
    lv_obj_set_scroll_chain(p->content,false);lv_obj_scroll_to_y(p->content,0,LV_ANIM_OFF);
}
static void tab_click(lv_event_t *event)
{
    SystemInfoPage *p=lv_event_get_user_data(event);
    p->tab=(unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event));p->selected=0;
    for(unsigned i=0;i<4U;++i) {
        /* RGB565的5位红色向下截位后，E8会在预览展开中偏红；E0更接近确认的浅蓝。 */
        lv_obj_set_style_bg_color(p->tabs[i],ui_color(i==p->tab?0xE0F3FD:UI_PANEL),0);
        lv_obj_set_style_border_color(p->tabs[i],ui_color(i==p->tab?0xBDDAF5:UI_BORDER),0);
        lv_obj_set_style_text_color(p->tabs[i],ui_color(i==p->tab?0x6A93BA:UI_INK),0);
        lv_obj_set_style_text_font(p->tabs[i],i==p->tab?&system_font_13_bold:&system_font_13,0);
    }
    build_content(p);system_info_refresh(p);
}
void system_info_create(SystemInfoPage *p,lv_obj_t *parent,bool persistent)
{
    p->root=area(parent,0,0,466,246);p->tab=0;p->selected=0;p->persistent=persistent;
    const char *names[]={"概览","任务","内存","设备"};
    for(unsigned i=0;i<4U;++i) {
        lv_obj_t *b=lv_button_create(p->root);p->tabs[i]=b;lv_obj_remove_style_all(b);
        lv_obj_set_pos(b,8+(int)i*114,0);lv_obj_set_size(b,108,31);
        lv_obj_set_style_bg_opa(b,255,0);lv_obj_set_style_bg_color(b,ui_color(i?UI_PANEL:0xE0F3FD),0);
        lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,ui_color(i?UI_BORDER:0xBDDAF5),0);
        lv_obj_set_style_radius(b,7,0);lv_obj_set_style_text_color(b,ui_color(i?UI_INK:0x6A93BA),0);
        lv_obj_set_style_text_font(b,i?&system_font_13:&system_font_13_bold,0);
        lv_obj_t *label=lv_label_create(b);lv_label_set_text(label,names[i]);lv_obj_center(label);
        lv_obj_set_user_data(b,(void *)(uintptr_t)i);lv_obj_add_event_cb(b,tab_click,LV_EVENT_CLICKED,p);
    }
    p->content=area(p->root,0,38,466,208);
    p->last_refresh_ms=lv_tick_get();
    p->snapshot=p->read?p->read(p->context):NULL;lv_mem_monitor(&p->memory);
    build_content(p);
}
void system_info_refresh(SystemInfoPage *p)
{
    if(!p->root)return;
    p->last_refresh_ms=lv_tick_get();
    p->snapshot=p->read?p->read(p->context):NULL;lv_mem_monitor(&p->memory);
    update_rows(p);lv_obj_invalidate(p->content);
}
void system_info_detach(SystemInfoPage *p)
{p->root=NULL;p->content=NULL;p->list=NULL;p->title=NULL;p->row_count=0;}
bool system_info_back(SystemInfoPage *p)
{
    if(!p->root||!p->selected)return false;
    p->selected=0;build_content(p);return true;
}
