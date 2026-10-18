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
