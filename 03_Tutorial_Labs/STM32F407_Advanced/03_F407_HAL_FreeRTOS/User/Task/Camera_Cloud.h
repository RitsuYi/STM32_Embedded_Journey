#ifndef CAMERA_CLOUD_H
#define CAMERA_CLOUD_H
#include <stdbool.h>
#include <stdint.h>

/* 云端只传普通数值，GUI任务才有权修改模型/LVGL。
 * values顺序固定：测量模式、工件类型、矩形宽/高、圆环外/内径、上/下偏差。
 * 尺寸均为0.01mm整数；mask的每一位表示本次是否修改对应项。 */
enum { CAMERA_CLOUD_SET=1, CAMERA_CLOUD_START=2, CAMERA_CLOUD_STOP=3 };
typedef struct {
    uint32_t camera_session, peer_session, token, received_ms;
    uint32_t values[8];
    uint16_t mask;
    uint8_t operation;
} CameraCloudCommand;
typedef struct {
    uint32_t values[8], total, passed, failed, uptime_ms;
    uint16_t error;
    uint8_t status, progress, trigger;
} CameraCloudState;
typedef struct {
    uint32_t camera_session, peer_session, token;
    uint16_t code; /* 200执行成功；409忙；422参数/组合无效；503设备不可用。 */
} CameraCloudReply;

/* 均是零等待的静态单槽；命令不覆盖，状态和已执行回执保留最新值。 */
bool Camera_CloudRead(CameraCloudCommand *command);
void Camera_CloudReply(const CameraCloudReply *reply);
void Camera_CloudPublish(const CameraCloudState *state);
#endif
