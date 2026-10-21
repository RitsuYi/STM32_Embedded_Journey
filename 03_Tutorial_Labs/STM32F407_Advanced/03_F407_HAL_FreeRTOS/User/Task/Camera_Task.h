#ifndef CAMERA_TASK_H
#define CAMERA_TASK_H

#include <stdint.h>

/* F407每个栈元素占4字节，512个元素约2KiB，从既有FreeRTOS堆分配。 */
#define CAMERA_TASK_STACK_WORDS 512U

/* 固定大小的调试状态，不增加采集任务或长期历史。
 * ready仅本地接收就绪，online需握手后收到匹配的双向探测应答。 */
typedef struct
{
    uint32_t ready;
    uint32_t received_bytes;
    uint32_t receive_errors; /* 接收中断或启动未就绪次数，不是丢失字节数。 */
    uint8_t last_byte;
    uint32_t received_frames, frame_errors, online; /* online需握手及有效PONG，不能用ready代替。 */
    /* 只保留累计次数，便于排障，不分配历史日志或新任务。
     * matched_pongs持续增长说明双向探测成功；transmit_errors为本地发送失败。
     * link_timeouts增长而receive_errors不变，表示一秒内没有有效探测应答；
     * 它只能定位超时分支，不能单独证明相机、线路或主控哪端损坏。 */
    uint32_t matched_pongs, transmit_errors, link_timeouts;
} CameraTaskStatus;

extern volatile CameraTaskStatus g_camera_task;

/* 相机任务入口，参数暂不使用；负责启动、维护和消费相机通信数据。 */
void vCamera_Task(void *argument);

#endif
