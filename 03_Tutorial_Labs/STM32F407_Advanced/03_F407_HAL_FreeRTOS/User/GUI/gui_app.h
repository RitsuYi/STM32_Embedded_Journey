#ifndef GUI_APP_H
#define GUI_APP_H
#include "instrument/dashboard_ui.h"

/* Persistence is optional. A future Flash implementation must report durable
 * success only after write/verification; NULL ops means volatile RAM only. */
typedef struct {
    bool (*load)(void *context, InstrumentModel *model);
    bool (*save)(void *context, const InstrumentModel *model);
    void *context;
    const char *name;
} GUI_StorageOps;
void GUI_AppInit(const GUI_StorageOps *storage);
void GUI_AppKey(unsigned key);
void GUI_AppProcess(void);
/* 以下接口仅由GUI任务调用；其他任务应先通过队列投递，不能跨任务操作LVGL。 */
void GUI_AppSetNetwork(const DashboardNetworkOps *ops);
void GUI_AppNetworkStatus(bool connected, const char *ssid, const char *ip);
void GUI_AppNetworkUpdate(const CameraNetworkState *state);
#endif
