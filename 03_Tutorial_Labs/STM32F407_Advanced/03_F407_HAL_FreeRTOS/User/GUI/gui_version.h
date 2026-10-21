#ifndef GUI_VERSION_H
#define GUI_VERSION_H

/* 固件版本只在此处维护，设置入口、更新页和设备信息共用同一个字符串。
 * 后续接入OTA（通过网络下载并升级固件）时，也应从这里取得本机版本，
 * 不能仅凭版本文字就认定已连接更新服务器或已完成升级。 */
#define GUI_FIRMWARE_VERSION "1.0.0"

#endif
