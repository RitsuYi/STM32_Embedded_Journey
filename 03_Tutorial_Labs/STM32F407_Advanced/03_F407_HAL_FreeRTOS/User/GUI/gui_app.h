#ifndef GUI_APP_H
#define GUI_APP_H
#include "instrument/dashboard_ui.h"
#include "../Task/Camera_Cloud.h"
#include "../Task/Camera_Upload.h"

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
/* 在GUI任务初始化时绑定上传回调；回调参数只在调用期间有效，后端必须立即复制。 */
void GUI_AppSetUpload(bool (*submit)(void *context, const InstrumentRecord *record), void *context);
/* 绑定系统统计只读数据源；回调必须在GUI任务内立即返回，不等待其他任务。 */
void GUI_AppSetSystemSource(SystemInfoRead read,void *context);
/* 仅在GUI任务执行：参数先整体验证再提交，不允许部分修改成功。 */
uint16_t GUI_AppCloudCommand(const CameraCloudCommand *command);
void GUI_AppCloudState(CameraCloudState *state);
void GUI_AppUploadSnapshot(CameraUploadRecord *snapshot);
#endif
