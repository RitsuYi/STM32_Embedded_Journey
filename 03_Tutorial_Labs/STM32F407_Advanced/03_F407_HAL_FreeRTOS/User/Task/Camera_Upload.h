#ifndef CAMERA_UPLOAD_H
#define CAMERA_UPLOAD_H

#include <stdbool.h>
#include <stdint.h>

/* 单条业务快照，不含LVGL指针。GUI立即复制入队，Camera任务独占串口。
 * 尺寸单位0.01mm；rectangle为1表示矩形、0表示圆环。
 * verdict：0普通测量未判定、1合格、2不合格；模拟来源必须原样保留。 */
typedef struct {
    uint32_t record_id, uptime_ms;
    int32_t primary_cmm, secondary_cmm;
    uint8_t rectangle, verdict, simulated;
    /* extended=1使用紧凑二进制扩展帧；保留旧JSON帧供旧探针联调。
     * 这是完成时复制的配置/统计，随后改标准不会篡改这条历史。 */
    uint8_t extended, trigger;
    uint32_t duration_ms, targets[4], upper_cmm, lower_cmm;
    uint32_t total, passed, failed;
} CameraUploadRecord;

/* 与相机RESULT.upload_state一致；ACK仅受理，只有ACCEPTED才表示平台接受。 */
typedef enum {
    CAMERA_UPLOAD_IDLE, CAMERA_UPLOAD_QUEUED, CAMERA_UPLOAD_SENDING,
    CAMERA_UPLOAD_MQTT_ACKED, CAMERA_UPLOAD_ACCEPTED, CAMERA_UPLOAD_REJECTED,
    CAMERA_UPLOAD_UNCONFIRMED, CAMERA_UPLOAD_FAILED
} CameraUploadState;

/* 仅任务中调用，立即返回：true只表示进入4条静态队列，false表示未排入。
 * 断电不保留；队满不会覆盖旧记录，不增加任务或等待GUI。 */
bool Camera_UploadSubmit(const CameraUploadRecord *record);

#endif
