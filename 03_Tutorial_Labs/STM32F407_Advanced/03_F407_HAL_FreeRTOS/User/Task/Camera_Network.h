#ifndef CAMERA_NETWORK_H
#define CAMERA_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

/* 两个任务交换普通数据，不在串口任务中传递或操作LVGL对象。
 * 只保留信号最强的5个热点，每个SSID最多32个UTF-8字节，末尾另留结束符。 */
#define CAMERA_AP_LIMIT 5U
typedef struct {
    char ssid[33];
    int8_t rssi;              /* 无线信号强度，单位dBm；通常负数，越接近0越强。 */
    uint8_t secured;          /* 1需要密码；0为开放网络。 */
} CameraAccessPoint;

enum { CAMERA_WIFI_UNKNOWN, CAMERA_WIFI_OFFLINE, CAMERA_WIFI_CONNECTING,
       CAMERA_WIFI_SWITCHING, CAMERA_WIFI_DISCONNECTING, CAMERA_WIFI_CONNECTED };
enum { CAMERA_SCAN_IDLE, CAMERA_SCAN_WAITING, CAMERA_SCAN_RUNNING,
       CAMERA_SCAN_DONE, CAMERA_SCAN_FAILED };
enum { CAMERA_NET_OK, CAMERA_NET_LINK_LOST, CAMERA_NET_REJECTED,
       CAMERA_NET_TIMEOUT, CAMERA_NET_SCAN_ERROR, CAMERA_NET_INCOMPLETE };
typedef struct {
    CameraAccessPoint ap[CAMERA_AP_LIMIT];
    char ssid[33], ip[16];
    uint8_t online, wifi, scan, count, error;
} CameraNetworkState;

/* main在创建任务前准备两个静态单槽队列，不占用新增RTOS堆。
 * 请求立即复制SSID/密码；false表示忙或未准备好，调用方不能等待GUI线程。 */
bool Camera_NetworkPrepare(void);
bool Camera_RequestScan(void);
bool Camera_RequestConnect(const char *ssid, const char *password);
bool Camera_RequestDisconnect(void);
/* 只有GUI任务取状态；只保留最新快照，慢GUI不会积压旧列表。 */
bool Camera_NetworkRead(CameraNetworkState *state);

#endif
