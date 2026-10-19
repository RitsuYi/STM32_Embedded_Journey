#include "dashboard_ui.h"
#include "../measurement/ui_draw.h"
#include "../fonts/measurement_fonts.h"
#include "src/core/lv_obj_event_private.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

/* 二级页面共用一个可垂直滚动的容器，每次仅创建当前菜单。
 * 历史采用8条一段的滚动列表，64条记录不会同时占用LVGL的48KB内存池。
 * 页面从右侧滑入/滑出，列表保留惯性滑动，手指拖动不会误当成按钮点击。 */
enum { EDIT_OFF, EDIT_TARGET, EDIT_SAMPLES_NEW, EDIT_TIMEOUT_NEW, EDIT_RESIDUAL_NEW,
       EDIT_SSID, EDIT_PASSWORD };
static void render(DashboardUI *u);
static void apply(DashboardUI *u);
static void row_event(lv_event_t *e);
/* 历史表格一行只占一个按钮对象，五列文字在绘图时按固定列宽绘制，
 * 不为64条历史各分配五个标签，减少F407内存和菜单滑入时的绘图峰值。 */
static void draw_record(lv_event_t *e)
{
    DashboardUI *u=lv_event_get_user_data(e);lv_obj_t *o=lv_event_get_target_obj(e);
    unsigned i=(unsigned)(uintptr_t)lv_obj_get_user_data(o);if(i>=u->model->record_count)return;
    const InstrumentRecord *r=&u->model->records[i];lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *l=lv_event_get_layer(e);char v[40];int x=a.x1,y=a.y1+10;
    snprintf(v,sizeof(v),"#%" PRIu32,r->id);ui_text(l,x+10,y,48,&ui_font_12,UI_INK,v,LV_TEXT_ALIGN_LEFT);
    snprintf(v,sizeof(v),"%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,r->uptime_ms/3600000U,r->uptime_ms/60000U%60U,r->uptime_ms/1000U%60U);
    ui_text(l,x+62,y,99,&ui_font_12,UI_MUTED,v,LV_TEXT_ALIGN_LEFT);
    measurement_format_cmm(v,sizeof(v),(uint32_t)r->result.primary_cmm);ui_text(l,x+167,y,68,&ui_font_12,UI_CYAN,v,LV_TEXT_ALIGN_LEFT);
    measurement_format_cmm(v,sizeof(v),(uint32_t)r->result.secondary_cmm);ui_text(l,x+251,y,68,&ui_font_12,UI_PURPLE,v,LV_TEXT_ALIGN_LEFT);
    ui_text(l,x+335,y,100,&ui_font_12,r->verdict==INSPECTION_PASS?UI_GREEN:UI_INK,
        r->inspection?(r->verdict==INSPECTION_PASS?"合格  >":"超差  >"):"普通测量  >",LV_TEXT_ALIGN_LEFT);
}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,ui_color(color),0);lv_obj_set_style_bg_opa(o,255,0);
    lv_obj_set_scrollable(o,false);return o;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *s,bool bold,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    /* 二级页空间充足：说明文字由11px提升到12px，粗体标题保持13px。 */
    lv_obj_set_style_text_font(o,bold?&ui_font_14_bold:&ui_font_12,0);
    lv_obj_set_style_text_color(o,ui_color(color),0);lv_label_set_text(o,s);
    lv_obj_set_clickable(o,false);return o;
}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,int h,const char *s,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_button_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_opa(o,255,0);lv_obj_set_style_bg_color(o,ui_color(UI_PANEL),0);
    lv_obj_set_style_bg_color(o,ui_color(0xD9EDFF),LV_STATE_PRESSED);
    lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,ui_color(UI_BORDER),0);
    lv_obj_set_style_radius(o,8,0);lv_obj_set_style_text_color(o,ui_color(UI_INK),0);
    lv_obj_set_style_text_font(o,&ui_font_14_bold,0);
    lv_obj_t *t=lv_label_create(o);lv_label_set_text(t,s);lv_obj_center(t);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data);return o;
}
static void note(DashboardUI *u,const char *s)
{
    snprintf(u->message,sizeof(u->message),"%s",s);
    measurement_page_show_notice(u->page,s,false);
}
static bool idle(DashboardUI *u)
{
    if(!measurement_session_busy(&u->page->session)&&!u->page->automatic)return true;
    note(u,"请先停止检测再修改参数");return false;
}
static bool persist(DashboardUI *u)
{
    if(!u->services.save) {note(u,"已更新，仅内存保存");return true;}
    if(u->services.save(u->services.context,u->model)) {note(u,"设置已保存");return true;}
    *u->model=u->rollback;note(u,"保存失败，已恢复原设置");return false;
}
static const char *verdict_name(InstrumentVerdict v,MeasurementGeometry g)
{
    switch(v) {
    case INSPECTION_PASS:return "合格";
    case INSPECTION_PRIMARY:return g==MEASUREMENT_RING?"外径超差":"长度超差";
    case INSPECTION_SECONDARY:return g==MEASUREMENT_RING?"内径超差":"宽度超差";
    case INSPECTION_INVALID:return "数据无效";
    default:return "普通测量";
    }
}
static void summary(DashboardUI *u)
{
    MeasurementPage *p=u->page;InstrumentModel *m=u->model;
    p->total_count=m->inspected;p->pass_count=m->passed;p->fail_count=m->failed;
    memset(p->recent,0,sizeof(p->recent));unsigned n=0;
    memset(p->recent_records,0,sizeof(p->recent_records));
    for(unsigned i=0;i<m->record_count&&n<2;++i) {
        const InstrumentRecord *r=&m->records[i];if(!r->inspection)continue;
        char a[16],b[16];measurement_format_cmm(a,sizeof(a),(uint32_t)r->result.primary_cmm);
        measurement_format_cmm(b,sizeof(b),(uint32_t)r->result.secondary_cmm);
        p->recent_records[n].id=r->id;p->recent_records[n].uptime_ms=r->uptime_ms;
        p->recent_records[n].primary_cmm=r->result.primary_cmm;p->recent_records[n].secondary_cmm=r->result.secondary_cmm;
        p->recent_records[n].pass=r->verdict==INSPECTION_PASS;
        snprintf(p->recent[n++],sizeof(p->recent[0]),"#%" PRIu32 "  %s  %s  %s",r->id,a,b,
                 r->verdict==INSPECTION_PASS?"合格":"超差");
    }
    lv_obj_invalidate(p->result_panel);
}
static void completed(void *context,const MeasurementResult *result)
{
    DashboardUI *u=context;InstrumentModel *m=u->model;
    u->rollback=*m;
    if(!instrument_add_record(m,result)) {u->page->automatic=false;note(u,"记录失败，请检查数据");return;}
    InstrumentRecord *r=&m->records[0];r->uptime_ms=lv_tick_get();r->inspection=u->page->quality_mode;
    r->verdict=r->inspection?instrument_inspect(&m->settings,result):INSPECTION_NONE;
    u->page->verdict=r->verdict;
    /* 只有完成且通过数据校验的结果计入统计，超时/取消不会伪造一条检测。 */
    if(r->inspection&&m->inspected<UINT32_MAX) {
        /* 总数到上限后三项一起停止增长，保持总数=合格+不合格的关系。 */
        ++m->inspected;
        if(r->verdict==INSPECTION_PASS)++m->passed;
        else ++m->failed;
    }
    if(!persist(u))u->page->automatic=false;
    /* 结果完成后至少留2.5秒阅读时间，再请求下一件；使用期限比较，不阻塞GUI触摸。
     * 下一轮采集仍保留上一份有效结果，只有新结果完成才替换数值与结论。 */
    summary(u);u->next_auto_ms=lv_tick_get()+2500U;
    /* 保存失败会停止自动检测，同步按钮文字和禁用状态，不能留下“停止”假象。 */
    measurement_page_set_backend(u->page,&u->page->backend);
}
static void apply(DashboardUI *u)
{
    InstrumentSettings *s=&u->model->settings;
    measurement_page_select_geometry(u->page,s->mode);
    measurement_page_set_units(u->page,s->inches);
    geometry_view_set_options(&u->page->geometry,s->grid,s->dimensions,s->contour,s->inches);
    u->page->session.timeout_ms=s->timeout_ms;
    measurement_page_set_device(u->page,u->page->session.connected,instrument_calibration_ready(u->model));
}
/* 自动模式发起下一次请求的入口。真实相机接入后应由后端保证一件工件只回报一次；
 * 目前2.5秒阅读间隔只是模拟重复检测，不能当成产线工件去重或真实检测速度。 */
void dashboard_action(DashboardUI *u)
{
    if(!u||u->page->selected_navigation>=2)return;
    MeasurementPage *p=u->page;
    if(!p->quality_mode) {measurement_page_start(p);return;}
    p->automatic=!p->automatic;
    if(!p->automatic) {
        if(measurement_session_busy(&p->session)) {
            if(p->backend.cancel)p->backend.cancel(p->backend.context,p->session.request_id);
            p->session.state=MEASUREMENT_IDLE;p->session.progress=0;
        }
    } else if(!measurement_page_start(p))p->automatic=false;
    measurement_page_set_backend(p,&p->backend);lv_obj_invalidate(p->geometry.object);
}
static void primary(void *context) {dashboard_action(context);}
static void timer_tick(lv_timer_t *t)
{
    DashboardUI *u=lv_timer_get_user_data(t);MeasurementPage *p=u->page;
    uint32_t now=lv_tick_get();
    if(u->header_second!=now/1000U) {u->header_second=now/1000U;lv_obj_invalidate(p->header);}
    if(p->automatic&&!measurement_session_busy(&p->session)) {
        if(p->session.state==MEASUREMENT_FAILED||!p->session.connected) {
            p->automatic=false;measurement_page_set_backend(p,&p->backend);
        } else if(p->selected_navigation==1&&(int32_t)(now-u->next_auto_ms)>=0) {
            if(!measurement_page_start(p))p->automatic=false;
        }
    }
}
static void slide_x(void *object,int32_t x) {lv_obj_set_x(object,x);}
static void closed(lv_anim_t *a)
{
    DashboardUI *u=lv_anim_get_user_data(a);lv_obj_set_hidden(u->panel,true);u->closing=false;
}
static void slide(DashboardUI *u,bool close)
{
    lv_anim_delete(u->panel,slide_x);lv_anim_t a;lv_anim_init(&a);
    lv_anim_set_var(&a,u->panel);lv_anim_set_exec_cb(&a,slide_x);
    lv_anim_set_values(&a,close?lv_obj_get_x(u->panel):480,close?480:0);
    lv_anim_set_duration(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
    if(close) {lv_anim_set_user_data(&a,u);lv_anim_set_completed_cb(&a,closed);}
    lv_anim_start(&a);
}
void dashboard_open(DashboardUI *u,DashboardMenu menu)
{
    if(!u)return;
    bool hidden=lv_obj_is_hidden(u->panel);
    u->menu=menu;u->closing=false;lv_anim_delete(u->panel,slide_x);
    lv_obj_set_x(u->panel,0);lv_obj_set_hidden(u->panel,false);lv_obj_move_foreground(u->panel);
    render(u);lv_obj_scroll_to_y(u->body,0,LV_ANIM_OFF);
    if(hidden)slide(u,false);
}
void dashboard_close(DashboardUI *u)
{
    if(!u||u->closing)return;
    /* 恢复主画面后，覆盖层仍拦截触摸直到滑出完成，不会点穿到底部按钮。 */
    u->page->selected_navigation=u->last_main;
    lv_obj_set_hidden(u->page->geometry.object,false);lv_obj_set_hidden(u->page->result_panel,false);
    u->closing=true;slide(u,true);measurement_page_set_backend(u->page,&u->page->backend);
}
static void navigation(void *context,unsigned destination)
{
    DashboardUI *u=context;
    if(destination<2) {u->last_main=destination;u->page->verdict=INSPECTION_NONE;return;}
    if(destination==2) {u->record_offset=0;dashboard_open(u,DASH_RECORDS);}
    else dashboard_open(u,destination==4?DASH_STANDARDS:DASH_SETTINGS);
}
static void back_event(lv_event_t *e)
{
    DashboardUI *u=lv_event_get_user_data(e);
    if(u->menu==DASH_SETTINGS||u->menu==DASH_RECORDS||u->menu==DASH_STANDARDS)dashboard_close(u);
    else dashboard_open(u,u->menu==DASH_RECORD_DETAIL?DASH_RECORDS:DASH_SETTINGS);
}
/* 一行就是一个足够大的触摸目标；不扩出滚动容器，避免滑动与相邻行串触。 */
static lv_obj_t *row(DashboardUI *u,unsigned index,int y,const char *name,const char *value)
{
    lv_obj_t *o=button(u->body,8,y,448,39,"",row_event,u);
    lv_obj_delete(lv_obj_get_child(o,0));
    label(o,12,9,190,name,true,UI_INK);label(o,202,11,224,value,false,UI_MUTED);
    lv_obj_set_style_text_align(lv_obj_get_child(o,1),LV_TEXT_ALIGN_RIGHT,0);
    if(index<8)u->rows[index]=o;
    /* 用户数据保存稳定的行编号，回调中的u仍由事件user_data提供。 */
    lv_obj_set_user_data(o,(void *)(uintptr_t)index);return o;
}
static void open_editor(DashboardUI *u,unsigned edit,const char *title,const char *value)
{
    /* 编辑器完全遮住菜单，临时释放菜单行，给可见键盘的绘图留下48KB池内空间。
     * 保存/取消后按原滚动位置重建；不扩大F407的内存池。 */
    if(!u->edit)u->edit_scroll=lv_obj_get_scroll_y(u->body);
    lv_obj_clean(u->body);memset(u->rows,0,sizeof(u->rows));
    u->edit=edit;lv_label_set_text(u->editor_title,title);
    lv_label_set_text(u->editor_hint,edit>=EDIT_SSID?"仅预留联网接口，需后端回报连接状态":"数值单位mm，最多两位小数；右下确认保存");
    lv_textarea_set_accepted_chars(u->textarea,edit>=EDIT_SSID?NULL:"0123456789.");
    lv_textarea_set_max_length(u->textarea,edit==EDIT_SSID?32:edit==EDIT_PASSWORD?64:12);
    lv_textarea_set_password_mode(u->textarea,edit==EDIT_PASSWORD);
    lv_textarea_set_password_bullet(u->textarea,"*");
    lv_textarea_set_text(u->textarea,value);
    lv_keyboard_set_mode(u->keyboard,edit>=EDIT_SSID?LV_KEYBOARD_MODE_TEXT_LOWER:LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(u->keyboard,u->textarea);
    lv_obj_set_hidden(u->editor,false);lv_obj_move_foreground(u->editor);
}
static void close_editor(DashboardUI *u)
{
    u->edit=EDIT_OFF;lv_keyboard_set_textarea(u->keyboard,NULL);lv_obj_set_hidden(u->editor,true);
    /* 密码只为本次请求临时存在，关闭输入后清除显示缓冲，避免落入记录/日志。 */
    lv_textarea_set_text(u->textarea,"");
    render(u);lv_obj_scroll_to_y(u->body,u->edit_scroll,LV_ANIM_OFF);
}
static void keyboard_event(lv_event_t *e)
{
    DashboardUI *u=lv_event_get_user_data(e);
    if(lv_event_get_code(e)==LV_EVENT_CANCEL) {memset(u->password,0,sizeof(u->password));close_editor(u);return;}
    const char *s=lv_textarea_get_text(u->textarea);
    if(u->edit==EDIT_SSID) {
        if(!*s||strlen(s)>=sizeof(u->ssid)) {lv_label_set_text(u->editor_hint,"网络名称需为1至32字节");return;}
        snprintf(u->ssid,sizeof(u->ssid),"%s",s);open_editor(u,EDIT_PASSWORD,"Wi-Fi 密码","");return;
    }
    if(u->edit==EDIT_PASSWORD) {
        snprintf(u->password,sizeof(u->password),"%s",s);close_editor(u);
        bool accepted=u->network.connect&&u->network.connect(u->network.context,u->ssid,u->password);
        memset(u->password,0,sizeof(u->password));u->connecting=accepted;
        note(u,accepted?"连接请求已发送，等待设备回报":"联网接口未接入，未发起真实连接");render(u);return;
    }
    char *end;double n=strtod(s,&end);
    bool whole=u->edit==EDIT_SAMPLES_NEW||u->edit==EDIT_TIMEOUT_NEW;
    double factor=whole?1:100;
    if(!*s||*end||!isfinite(n)||n<0||n>10000||fabs(n*factor-round(n*factor))>1e-6) {
        lv_label_set_text(u->editor_hint,"输入无效，请检查范围和小数位数");return;
    }
    if(!idle(u)) {lv_label_set_text(u->editor_hint,u->message);return;}
    u->rollback=*u->model;InstrumentSettings *settings=&u->model->settings;
    uint32_t v=(uint32_t)lround(n*factor);
    if(u->edit==EDIT_TARGET) {
        InstrumentTolerance *t=&settings->tolerance[settings->mode][u->edit_dimension];
        if(u->edit_component==0)t->target_cmm=v;
        else if(u->edit_component==1)t->lower_cmm=v;else t->upper_cmm=v;
    } else if(u->edit==EDIT_SAMPLES_NEW)settings->sample_count=v;
    else if(u->edit==EDIT_TIMEOUT_NEW)settings->timeout_ms=v*1000U;
    else settings->quality_cmm=v;
    if(!instrument_settings_check(settings)) {
        *u->model=u->rollback;lv_label_set_text(u->editor_hint,"参数超范围；下偏差必须小于目标值");return;
    }
    if(!persist(u)) {lv_label_set_text(u->editor_hint,u->message);return;}
    apply(u);close_editor(u);render(u);
}
static void row_event(lv_event_t *e)
{
    DashboardUI *u=lv_event_get_user_data(e);
    unsigned i=(unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(e));
    char value[28];InstrumentSettings *s=&u->model->settings;
    if(u->menu==DASH_SETTINGS) {
        static const DashboardMenu dest[]={DASH_NETWORK,DASH_DEVICE,DASH_MEASUREMENT,DASH_DISPLAY,DASH_STANDARDS,DASH_RECORDS};
        if(i<6) {if(i==5)u->record_offset=0;dashboard_open(u,dest[i]);}return;
    }
    if(u->menu==DASH_NETWORK) {
        if(i==0)open_editor(u,EDIT_SSID,"Wi-Fi 网络名称",u->ssid);
        else if(i==1) {
            bool ok=u->network.disconnect&&u->network.disconnect(u->network.context);
            note(u,ok?"断开请求已发送，等待设备回报":"联网接口未接入");render(u);
        }
        return;
    }
    if(u->menu==DASH_RECORDS) {
        if(i==100) {u->record_offset=u->record_offset>=8?u->record_offset-8:0;render(u);lv_obj_scroll_to_y(u->body,0,LV_ANIM_ON);}
        else if(i==101) {u->record_offset+=8;render(u);lv_obj_scroll_to_y(u->body,0,LV_ANIM_ON);}
        else {u->detail_index=i;dashboard_open(u,DASH_RECORD_DETAIL);}return;
    }
    if(!idle(u)) {render(u);return;}
    if(u->menu==DASH_STANDARDS&&i<6) {
        u->edit_dimension=i/3;u->edit_component=i%3;
        const InstrumentTolerance *t=&s->tolerance[s->mode][i/3];
        measurement_format_cmm(value,sizeof(value),i%3==0?t->target_cmm:i%3==1?t->lower_cmm:t->upper_cmm);
        open_editor(u,EDIT_TARGET,i%3==0?"目标尺寸 (mm)":i%3==1?"下偏差绝对值 (mm)":"上偏差绝对值 (mm)",value);return;
    }
    if(u->menu==DASH_MEASUREMENT&&i>0&&i<4) {
        if(i==1)snprintf(value,sizeof(value),"%" PRIu32,s->sample_count);
        else if(i==2)snprintf(value,sizeof(value),"%" PRIu32,s->timeout_ms/1000U);
        else measurement_format_cmm(value,sizeof(value),s->quality_cmm);
        open_editor(u,i==1?EDIT_SAMPLES_NEW:i==2?EDIT_TIMEOUT_NEW:EDIT_RESIDUAL_NEW,
            i==1?"采样点数 32~512":i==2?"超时秒数 2~30":"残差阈值 0.01~1.00mm",value);return;
    }
    u->rollback=*u->model;
    if(u->menu==DASH_MEASUREMENT&&i==0)s->mode=s->mode==MEASUREMENT_RING?MEASUREMENT_RECTANGLE:MEASUREMENT_RING;
    else if(u->menu==DASH_DISPLAY) {
        if(i==0)s->grid=!s->grid;else if(i==1)s->dimensions=!s->dimensions;
        else if(i==2)s->contour=!s->contour;else if(i==3)s->inches=!s->inches;
    } else return;
    persist(u);apply(u);render(u);
}
static void render(DashboardUI *u)
{
    /* 删除的是当前菜单的LVGL对象，涉及内存释放，不是删除任何磁盘文件。 */
    lv_obj_clean(u->body);memset(u->rows,0,sizeof(u->rows));
    static const char *titles[]={"设置","Wi-Fi 网络","设备信息","测量参数","显示设置","标准设定","最近检测记录","检测记录详情"};
    lv_label_set_text(u->title,titles[u->menu]);char value[128],a[20],b[20];
    InstrumentModel *m=u->model;InstrumentSettings *s=&m->settings;
    int y=4;
    if(u->menu==DASH_SETTINGS) {
        const char *names[]={"Wi-Fi 网络","设备信息","测量参数","显示设置","标准设定","最近检测记录"};
        const char *vals[]={u->connected?u->ssid:"未连接  >","固件 / 屏幕 / 相机  >","工件 / 采样 / 超时  >","网格 / 标注 / 单位  >","目标尺寸与上下公差  >","滑动查看历史  >"};
        for(unsigned i=0;i<6;++i) {row(u,i,y,names[i],vals[i]);y+=45;}
    } else if(u->menu==DASH_NETWORK) {
        label(u->body,16,y,430,u->connected?"当前状态：已连接":u->connecting?"当前状态：连接中":"当前状态：未连接",true,UI_INK);y+=28;
        snprintf(value,sizeof(value),"SSID：%s",u->connected?u->ssid:"--");label(u->body,16,y,430,value,false,UI_MUTED);y+=23;
        snprintf(value,sizeof(value),"IP：%s",u->connected?u->ip:"--");label(u->body,16,y,430,value,false,UI_MUTED);y+=29;
        row(u,0,y,"连接Wi-Fi","输入网络名称和密码  >");y+=46;
        row(u,1,y,"断开网络",u->network.disconnect?"发送断开请求":"接口预留");y+=47;
        label(u->body,16,y,425,u->message[0]?u->message:"尚未接入联网模块，不显示虚构热点或连接成功。",false,UI_MUTED);y+=49;
        label(u->body,16,y,425,"联网后由后端回报SSID和IP；密码不写入检测记录。",false,UI_MUTED);
    } else if(u->menu==DASH_DEVICE) {
        const char *names[]={"设备名称","主控","显示屏","触摸","图形库","视觉前端","存储","运行时间"};
        snprintf(value,sizeof(value),"%" PRIu32 " 秒",lv_tick_get()/1000U);
        const char *vals[]={"智能视觉测量仪","STM32F407VGT6 / 168MHz","ST7796 / 480 x 320 / RGB565","FT6336U / I2C2","LVGL 9.6.0 / UI 2.0",
            u->page->session.simulated?"模拟数据 / MaixCAM2待接入":"MaixCAM2",u->services.persistent?"后端已接入":"仅内存 / 重启后清空",value};
        for(unsigned i=0;i<8;++i) {row(u,i,y,names[i],vals[i]);y+=45;lv_obj_set_clickable(u->rows[i],false);}
    } else if(u->menu==DASH_MEASUREMENT) {
        row(u,0,y,"工件类型",s->mode==MEASUREMENT_RING?"圆环  >":"矩形  >");y+=45;
        snprintf(value,sizeof(value),"%" PRIu32 " 点  >",s->sample_count);row(u,1,y,"采样点数",value);y+=45;
        snprintf(value,sizeof(value),"%" PRIu32 " 秒  >",s->timeout_ms/1000U);row(u,2,y,"检测超时",value);y+=45;
        measurement_format_cmm(a,sizeof(a),s->quality_cmm);snprintf(value,sizeof(value),"%s mm  >",a);row(u,3,y,"拟合残差阈值",value);y+=49;
        label(u->body,16,y,420,"相机固定参数在代码中配置，无现场标定菜单。",false,UI_MUTED);
    } else if(u->menu==DASH_DISPLAY) {
        const char *names[]={"显示网格","显示尺寸标注","显示工件轮廓","尺寸单位"};
        const char *vals[]={s->grid?"已开启":"已关闭",s->dimensions?"已开启":"已关闭",s->contour?"已开启":"已关闭",s->inches?"in":"mm"};
        for(unsigned i=0;i<4;++i) {row(u,i,y,names[i],vals[i]);y+=45;}
        label(u->body,16,y,420,"点击切换；主页面缩放按钮只改变预览比例，不改变测量值。",false,UI_MUTED);
    } else if(u->menu==DASH_STANDARDS) {
        label(u->body,16,y,428,s->mode==MEASUREMENT_RING?"圆环标准 (mm) · 区间含上下边界":"矩形标准 (mm) · 区间含上下边界",true,UI_INK);y+=28;
        for(unsigned i=0;i<6;++i) {
            const InstrumentTolerance *t=&s->tolerance[s->mode][i/3];
            const char *dimension=s->mode==MEASUREMENT_RING?(i<3?"外径":"内径"):(i<3?"长度":"宽度");
            snprintf(value,sizeof(value),"%s%s",dimension,i%3==0?"目标值":i%3==1?"下偏差":"上偏差");
            measurement_format_cmm(a,sizeof(a),i%3==0?t->target_cmm:i%3==1?t->lower_cmm:t->upper_cmm);
            row(u,i,y,value,a);y+=45;
        }
        label(u->body,16,y,420,"允许范围 = 目标值 - 下偏差 至 目标值 + 上偏差。\n只影响此后检测，历史保留原判定。",false,UI_MUTED);
    } else if(u->menu==DASH_RECORDS) {
        snprintf(value,sizeof(value),"共%" PRIu32 "条记录 · %s · 上滑查看更多",m->record_count,u->services.persistent?"后端存储":"仅内存");
        label(u->body,16,y,430,value,false,UI_MUTED);y+=27;
        label(u->body,20,y,425,"编号     检测时间(开机)      外径/长      内径/宽       结果",false,UI_INK);y+=25;
        if(!m->record_count)label(u->body,16,y,430,"暂无检测记录，请先进行测量或质检。",true,UI_MUTED);
        unsigned end=LV_MIN(m->record_count,u->record_offset+8U);
        for(unsigned i=u->record_offset;i<end;++i) {
            const InstrumentRecord *r=&m->records[i];
            lv_obj_t *o=button(u->body,8,y,448,37,"",row_event,u);lv_obj_set_user_data(o,(void *)(uintptr_t)i);
            lv_obj_delete(lv_obj_get_child(o,0));lv_obj_add_event_cb(o,draw_record,LV_EVENT_DRAW_MAIN,u);
            lv_obj_set_style_text_font(o,&ui_font_12,0);
            if(r->inspection)lv_obj_set_style_bg_color(o,ui_color(r->verdict==INSPECTION_PASS?0xE8FAF0:0xFFF0F3),0);
            y+=42;
        }
        if(u->record_offset) {row(u,100,y,"上一段","向前8条  >");y+=45;}
        if(end<m->record_count) {row(u,101,y,"更多记录","继续8条  >");y+=45;}
    } else if(u->detail_index<m->record_count) {
        const InstrumentRecord *r=&m->records[u->detail_index];
        snprintf(value,sizeof(value),"记录 # %" PRIu32,r->id);label(u->body,16,y,430,value,true,UI_INK);y+=34;
        label(u->body,16,y,430,verdict_name(r->verdict,r->result.geometry),true,r->verdict==INSPECTION_PASS?UI_GREEN:UI_INK);y+=32;
        measurement_format_cmm(a,sizeof(a),(uint32_t)r->result.primary_cmm);measurement_format_cmm(b,sizeof(b),(uint32_t)r->result.secondary_cmm);
        snprintf(value,sizeof(value),"%s：%s mm\n%s：%s mm",r->result.geometry==MEASUREMENT_RING?"外径":"长度",a,r->result.geometry==MEASUREMENT_RING?"内径":"宽度",b);
        label(u->body,16,y,430,value,true,UI_INK);y+=60;
        snprintf(value,sizeof(value),"时间：开机后%" PRIu32 "秒\n来源：%s\n模式：%s",r->uptime_ms/1000U,r->result.simulated?"模拟测量":"相机测量",r->inspection?"质量检测":"普通测量");
        label(u->body,16,y,430,value,false,UI_MUTED);
    }
    /* 底部保留可滚动空白，让最后一行也能被手指完整触及。 */
    lv_obj_update_layout(u->body);
}

/* 沿用已验收的数字键盘底行补偿；只给Y313～319新增区选原键，
 * 字符输入仍由LVGL键盘处理。文字键盘使用自身原生布局和命中。 */
static uint32_t bottom_key(lv_obj_t *k,const lv_point_t *p)
{
    const lv_buttonmatrix_t *m=(const lv_buttonmatrix_t *)k;lv_area_t a;lv_obj_get_coords(k,&a);
    if(lv_keyboard_get_mode(k)!=LV_KEYBOARD_MODE_NUMBER||m->btn_cnt<5||!m->button_areas||
       p->y<=a.y2||p->y>319||p->x<a.x1||p->x>a.x2)return LV_BUTTONMATRIX_BUTTON_NONE;
    unsigned first=m->btn_cnt-5;
    for(unsigned i=first;i<m->btn_cnt;++i) {
        int left=i==first?a.x1:a.x1+(m->button_areas[i-1].x2+m->button_areas[i].x1)/2+1;
        int right=i+1==m->btn_cnt?a.x2:a.x1+(m->button_areas[i].x2+m->button_areas[i+1].x1)/2;
        if(p->x<left||p->x>right)continue;
        if(lv_buttonmatrix_has_button_ctrl(k,i,LV_BUTTONMATRIX_CTRL_DISABLED)||lv_buttonmatrix_has_button_ctrl(k,i,LV_BUTTONMATRIX_CTRL_HIDDEN))return LV_BUTTONMATRIX_BUTTON_NONE;
        return i;
    }
    return LV_BUTTONMATRIX_BUTTON_NONE;
}
static void keyboard_bottom(lv_event_t *e)
{
    DashboardUI *u=lv_event_get_user_data(e);lv_obj_t *k=lv_event_get_target_obj(e);lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t *h=lv_event_get_hit_test_info(e);lv_area_t a;lv_obj_get_coords(k,&a);
        h->res=(h->point->x>=a.x1&&h->point->x<=a.x2&&h->point->y>=a.y1&&h->point->y<=a.y2)||bottom_key(k,h->point)!=LV_BUTTONMATRIX_BUTTON_NONE;return;
    }
    if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST) {u->keyboard_pressed_key=LV_BUTTONMATRIX_BUTTON_NONE;return;}
    if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESSING)return;
    lv_indev_t *input=lv_event_get_indev(e);if(!input)return;lv_point_t p;lv_indev_get_point(input,&p);
    uint32_t key=bottom_key(k,&p);
    if(code==LV_EVENT_PRESSED) {
        u->keyboard_pressed_key=lv_buttonmatrix_get_selected_button(k);if(key==LV_BUTTONMATRIX_BUTTON_NONE)return;
        u->keyboard_pressed_key=key;lv_buttonmatrix_set_selected_button(k,key);lv_obj_send_event(k,LV_EVENT_VALUE_CHANGED,&key);
    } else if(key!=LV_BUTTONMATRIX_BUTTON_NONE&&key==u->keyboard_pressed_key)lv_buttonmatrix_set_selected_button(k,key);
    else u->keyboard_pressed_key=lv_buttonmatrix_get_selected_button(k);
}
void dashboard_create(DashboardUI *u,MeasurementPage *p,InstrumentModel *m,const InstrumentServices *services)
{
    memset(u,0,sizeof(*u));u->page=p;u->model=m;if(services)u->services=*services;
    p->navigation_changed=navigation;p->result_completed=completed;p->application_context=u;p->primary_action=primary;
    u->panel=box(p->root,0,30,480,290,UI_BG);lv_obj_set_hidden(u->panel,true);
    u->back=button(u->panel,9,4,62,29,"< 返回",back_event,u);
    u->title=label(u->panel,88,9,377,"设置",true,UI_INK);
    u->body=box(u->panel,7,39,466,246,UI_BG);lv_obj_set_scrollable(u->body,true);
    lv_obj_set_scroll_dir(u->body,LV_DIR_VER);lv_obj_set_scrollbar_mode(u->body,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_momentum(u->body,true);lv_obj_set_scroll_elastic(u->body,false);lv_obj_set_scroll_chain(u->body,false);
    lv_obj_set_style_pad_bottom(u->body,14,0);
    u->editor=box(p->root,0,0,480,320,UI_BG);lv_obj_set_hidden(u->editor,true);
    u->editor_title=label(u->editor,12,8,454,"输入参数",true,UI_INK);
    u->editor_hint=label(u->editor,12,34,454,"",false,UI_MUTED);
    u->textarea=lv_textarea_create(u->editor);lv_obj_set_pos(u->textarea,12,70);lv_obj_set_size(u->textarea,456,40);
    lv_textarea_set_one_line(u->textarea,true);lv_obj_set_style_text_font(u->textarea,&ui_font_14_bold,0);
    /* 输入框也使用屏幕主题的柔和深色，避免正文变浅而编辑数字仍是默认纯黑。 */
    lv_obj_set_style_text_color(u->textarea,ui_color(UI_INK),0);
    lv_obj_set_style_text_color(u->textarea,ui_color(UI_MUTED),LV_PART_TEXTAREA_PLACEHOLDER);
    u->keyboard=lv_keyboard_create(u->editor);
    /* 键盘默认靠下对齐，必须先改为左上定位；否则y=121是从底部偏移，会超出屏幕。 */
    lv_obj_set_align(u->keyboard,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(u->keyboard,7,121);lv_obj_set_size(u->keyboard,466,192);
    lv_obj_set_style_bg_color(u->keyboard,ui_color(UI_BG),0);lv_obj_set_style_bg_color(u->keyboard,ui_color(UI_PANEL),LV_PART_ITEMS);
    lv_obj_set_style_text_color(u->keyboard,ui_color(UI_INK),LV_PART_ITEMS);
    /* 键盘的确认/切换键使用CHECKED样式，默认主题会把它们重新画成深灰黑。
     * 单独覆盖该状态及按下状态，让特殊键也沿用浅色主题；按键含义不变。 */
    lv_obj_set_style_bg_color(u->keyboard,ui_color(0xEAECE9),LV_PART_ITEMS|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(u->keyboard,ui_color(UI_INK),LV_PART_ITEMS|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(u->keyboard,ui_color(0xDCEBFA),LV_PART_ITEMS|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(u->keyboard,ui_color(UI_INK),LV_PART_ITEMS|LV_STATE_PRESSED);
    lv_keyboard_set_mode(u->keyboard,LV_KEYBOARD_MODE_NUMBER);lv_keyboard_set_textarea(u->keyboard,NULL);
    lv_obj_set_ext_click_area(u->keyboard,7);lv_obj_set_adv_hittest(u->keyboard,true);
    u->keyboard_pressed_key=LV_BUTTONMATRIX_BUTTON_NONE;
    lv_obj_add_event_cb(u->keyboard,keyboard_bottom,LV_EVENT_ALL,u);
    lv_obj_add_event_cb(u->keyboard,keyboard_event,LV_EVENT_READY,u);lv_obj_add_event_cb(u->keyboard,keyboard_event,LV_EVENT_CANCEL,u);
    u->timer=lv_timer_create(timer_tick,100,u);LV_ASSERT_MALLOC(u->timer);apply(u);summary(u);
}
void dashboard_set_network(DashboardUI *u,const DashboardNetworkOps *ops)
{ if(u)u->network=ops?*ops:(DashboardNetworkOps){0}; }
void dashboard_network_status(DashboardUI *u,bool connected,const char *ssid,const char *ip)
{
    if(!u)return;
    u->connected=connected;u->connecting=false;u->page->wifi_connected=connected;
    snprintf(u->ssid,sizeof(u->ssid),"%s",ssid?ssid:"");snprintf(u->ip,sizeof(u->ip),"%s",ip?ip:"");
    lv_obj_invalidate(u->page->header);
    if(!u->edit&&!lv_obj_is_hidden(u->panel)&&u->menu==DASH_NETWORK)render(u);
}
void dashboard_destroy(DashboardUI *u)
{
    if(!u)return;
    lv_anim_delete(u->panel,slide_x);lv_timer_delete(u->timer);
    lv_obj_delete(u->panel);lv_obj_delete(u->editor);memset(u->password,0,sizeof(u->password));
}
