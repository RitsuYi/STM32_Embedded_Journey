#include "gui_app.h"
#include "gui_demo.h"
#include "gui_camera_config.h"

/* 整个界面与模型由唯一GUI任务拥有，继续沿用原显示/触摸/存储边界。 */
static MeasurementPage page;
static InstrumentModel model;
static DashboardUI ui;
static GUI_StorageOps storage;
static bool save(void *context,const InstrumentModel *value)
{
    (void)context;return storage.save&&storage.save(storage.context,value);
}
void GUI_AppInit(const GUI_StorageOps *ops)
{
    storage=ops?*ops:(GUI_StorageOps){0};instrument_defaults(&model);
    bool loaded=storage.load&&storage.load(storage.context,&model)&&instrument_model_check(&model);
    if(!loaded)instrument_defaults(&model);
    /* 没有现场标定入口，模拟后端使用集中配置的固定参数。 */
    GUI_ApplyFixedCamera(&model);
    MeasurementResult initial={.geometry=model.settings.mode,.status=MEASUREMENT_PENDING,
        .connected=true,.calibrated=instrument_calibration_ready(&model),.simulated=true};
    measurement_page_create(&page,lv_screen_active(),&initial);GUI_DemoInit(&page,&model);
    InstrumentServices services={.save=storage.save?save:NULL,
        .persistent=storage.save!=NULL,.storage_name=storage.name};
    dashboard_create(&ui,&page,&model,&services);
}
void GUI_AppKey(unsigned key)
{
    if(ui.edit)return;
    /* 实体键作为触摸的替代入口：KEY1切换模式、KEY2设置/返回、KEY3主动作。 */
    if(key==1) {
        if(!lv_obj_is_hidden(ui.panel)||measurement_session_busy(&page.session)||page.automatic)return;
        measurement_page_navigate(&page,page.quality_mode?0:1);
    } else if(key==2) {
        if(lv_obj_is_hidden(ui.panel))measurement_page_navigate(&page,3);else dashboard_close(&ui);
    } else if(key==3&&lv_obj_is_hidden(ui.panel))dashboard_action(&ui);
}
void GUI_AppProcess(void) {measurement_page_process(&page,lv_tick_get());}
void GUI_AppSetNetwork(const DashboardNetworkOps *ops) {dashboard_set_network(&ui,ops);}
void GUI_AppNetworkStatus(bool connected,const char *ssid,const char *ip)
{ dashboard_network_status(&ui,connected,ssid,ip); }
void GUI_AppNetworkUpdate(const CameraNetworkState *state) {dashboard_network_update(&ui,state);}
void GUI_AppSetUpload(bool (*submit)(void *context, const InstrumentRecord *record),void *context)
{ ui.services.upload=submit;ui.services.upload_context=context; }
void GUI_AppSetSystemSource(SystemInfoRead read,void *context)
{ui.system.read=read;ui.system.context=context;}

uint16_t GUI_AppCloudCommand(const CameraCloudCommand *command)
{ return dashboard_cloud_command(&ui,command); }
void GUI_AppCloudState(CameraCloudState *s)
{
    /* 从GUI所拥有的模型复制；通信任务绝不借用模型指针。累计统计沿用质检口径，
     * 仅内存有效、重启清零；普通测量不加入合格率的分母。 */
    const InstrumentSettings *v=&model.settings;
    *s=(CameraCloudState){0};
    s->values[0]=page.quality_mode;s->values[1]=v->mode==MEASUREMENT_RECTANGLE?0:2;
    for(unsigned i=2;i<6;++i)s->values[i]=v->tolerance[i<4?MEASUREMENT_RECTANGLE:MEASUREMENT_RING][i%2].target_cmm;
    s->values[6]=v->tolerance[v->mode][0].upper_cmm;s->values[7]=v->tolerance[v->mode][0].lower_cmm;
    s->total=model.inspected;s->passed=model.passed;s->failed=model.failed;
    s->uptime_ms=lv_tick_get();s->progress=page.session.progress;s->trigger=page.trigger_source;
    s->error=(uint16_t)page.session.error;
    /* 自动质检包括每件完成后的2.5秒阅读间隔，整轮结束前都属于运行中。
     * 否则网页会在两件之间误显示空闲；错误仍优先报告为故障。 */
    s->status=s->error?2:((page.automatic||measurement_session_busy(&page.session))?1:0);
}
void GUI_AppUploadSnapshot(CameraUploadRecord *s)
{
    /* 回调发生在有效记录完成时，此刻固定标准、耗时和触发来源；
     * 后续队列等待期间修改设置，也不能改变已经完成那件工件的历史。 */
    const InstrumentSettings *v=&model.settings;
    s->extended=1;s->trigger=page.trigger_source;
    s->duration_ms=s->uptime_ms-page.session.started_ms;
    if(s->duration_ms>600000U)s->duration_ms=600000U;
    for(unsigned i=0;i<4;++i)s->targets[i]=v->tolerance[i<2?MEASUREMENT_RECTANGLE:MEASUREMENT_RING][i%2].target_cmm;
    s->upper_cmm=v->tolerance[v->mode][0].upper_cmm;s->lower_cmm=v->tolerance[v->mode][0].lower_cmm;
    s->total=model.inspected;s->passed=model.passed;s->failed=model.failed;
}
