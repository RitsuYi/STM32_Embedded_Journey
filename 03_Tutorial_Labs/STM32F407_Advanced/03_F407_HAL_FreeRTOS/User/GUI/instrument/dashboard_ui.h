#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H
#include "instrument_ui.h"
#include "../../Task/Camera_Network.h"

/* 网络接口仅发请求，必须立即返回，不能在GUI里等待连接。
 * 后续串口任务通过队列把状态送回GUI任务，再调用dashboard_network_status。
 * true只表示请求已接受；真正“已连接”必须收到后端的状态回报。 */
typedef struct {
    bool (*connect)(void *context, const char *ssid, const char *password);
    bool (*disconnect)(void *context);
    void *context;
    bool (*scan)(void *context); /* 只提交刷新请求，不能在GUI回调里等待无线扫描。 */
} DashboardNetworkOps;
typedef enum { DASH_SETTINGS, DASH_NETWORK, DASH_DEVICE, DASH_MEASUREMENT,
               DASH_DISPLAY, DASH_STANDARDS, DASH_RECORDS, DASH_RECORD_DETAIL,
               DASH_UPDATE } DashboardMenu;
/* 当前8条历史的显示文字只在创建该段列表时转换一次。
 * 缓存属于GUI对象，条带绘图期间一直有效，不把临时栈字符串交给延后绘图。 */
typedef struct {
    char id[12], time[20], primary[16], secondary[16];
    InstrumentVerdict verdict;
    bool inspection; /* 与同一行文字一起缓存，过渡中记录插入时不会混用新旧结论。 */
} DashboardRecordText;
typedef struct {
    MeasurementPage *page;
    InstrumentModel *model;
    InstrumentModel rollback; /* 保存失败时回滚，避免把约2KB模型放到任务栈。 */
    InstrumentServices services;
    DashboardNetworkOps network;
    CameraNetworkState wireless; /* 固定5条热点，不为每次心跳新建列表缓存。 */
    lv_obj_t *ap_rows[CAMERA_AP_LIMIT];
    lv_obj_t *panel, *title, *body, *back, *transition_guard;
    lv_obj_t *editor, *editor_title, *editor_hint, *textarea, *keyboard;
    lv_obj_t *rows[8];
    DashboardRecordText record_text[8];
    lv_timer_t *timer;
    DashboardMenu menu;
    DashboardMenu rendered_menu; /* 当前对象实际对应哪个菜单，允许再次打开设置时复用原行。 */
    unsigned last_main, edit, edit_dimension, edit_component, detail_index, record_offset;
    uint32_t keyboard_pressed_key, next_auto_ms, header_second;
    uint32_t connect_started_ms; /* 用户请求被接受的时刻，复用GUI定时器计算15秒等待。 */
    int32_t edit_scroll;
    bool connected, connecting, closing;
    bool connect_requested; /* 超时后仍记住本次请求，防止后台“连接中”把提示重新打开。 */
    bool animating, menu_dirty, summary_pending;
    unsigned result_verdict; /* 已接收结果的真实结论；summary提交时才改变主页可见结论。 */
    /* 当前连接名与用户正在输入密码的目标名分开，心跳不能覆盖待连接SSID。 */
    char ssid[33], target_ssid[33], ip[40], password[65], message[96];
    char connecting_ssid[33]; /* 已发出的目标固定保存，随后编辑另一个SSID不会改写它。 */
} DashboardUI;

/* 创建新主界面业务层；复用原测量后端和存储接口，不调用旧标定向导。 */
void dashboard_create(DashboardUI *ui, MeasurementPage *page, InstrumentModel *model,
                      const InstrumentServices *services);
void dashboard_open(DashboardUI *ui, DashboardMenu menu);
void dashboard_close(DashboardUI *ui);
void dashboard_action(DashboardUI *ui);
void dashboard_set_network(DashboardUI *ui, const DashboardNetworkOps *ops);
void dashboard_network_status(DashboardUI *ui, bool connected, const char *ssid, const char *ip);
/* 只能由GUI任务调用；相同快照直接返回，避免每秒心跳重建整个网络页。 */
void dashboard_network_update(DashboardUI *ui, const CameraNetworkState *state);
void dashboard_destroy(DashboardUI *ui);
#endif
