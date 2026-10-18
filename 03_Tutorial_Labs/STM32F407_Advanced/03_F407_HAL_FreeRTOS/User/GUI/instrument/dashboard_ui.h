#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H
#include "instrument_ui.h"

/* 网络接口仅发请求，必须立即返回，不能在GUI里等待连接。
 * 后续串口任务通过队列把状态送回GUI任务，再调用dashboard_network_status。
 * true只表示请求已接受；真正“已连接”必须收到后端的状态回报。 */
typedef struct {
    bool (*connect)(void *context, const char *ssid, const char *password);
    bool (*disconnect)(void *context);
    void *context;
} DashboardNetworkOps;
typedef enum { DASH_SETTINGS, DASH_NETWORK, DASH_DEVICE, DASH_MEASUREMENT,
               DASH_DISPLAY, DASH_STANDARDS, DASH_RECORDS, DASH_RECORD_DETAIL } DashboardMenu;
typedef struct {
    MeasurementPage *page;
    InstrumentModel *model;
    InstrumentModel rollback; /* 保存失败时回滚，避免把约2KB模型放到任务栈。 */
    InstrumentServices services;
    DashboardNetworkOps network;
    lv_obj_t *panel, *title, *body, *back;
    lv_obj_t *editor, *editor_title, *editor_hint, *textarea, *keyboard;
    lv_obj_t *rows[8];
    lv_timer_t *timer;
    DashboardMenu menu;
    unsigned last_main, edit, edit_dimension, edit_component, detail_index, record_offset;
    uint32_t keyboard_pressed_key, next_auto_ms, header_second;
    int32_t edit_scroll;
    bool connected, connecting, closing;
    char ssid[33], ip[40], password[65], message[96];
} DashboardUI;

/* 创建新主界面业务层；复用原测量后端和存储接口，不调用旧标定向导。 */
void dashboard_create(DashboardUI *ui, MeasurementPage *page, InstrumentModel *model,
                      const InstrumentServices *services);
void dashboard_open(DashboardUI *ui, DashboardMenu menu);
void dashboard_close(DashboardUI *ui);
void dashboard_action(DashboardUI *ui);
void dashboard_set_network(DashboardUI *ui, const DashboardNetworkOps *ops);
void dashboard_network_status(DashboardUI *ui, bool connected, const char *ssid, const char *ip);
void dashboard_destroy(DashboardUI *ui);
#endif
