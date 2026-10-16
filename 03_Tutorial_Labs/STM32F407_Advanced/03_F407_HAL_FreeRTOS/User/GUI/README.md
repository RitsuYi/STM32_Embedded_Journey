# STM32 LVGL应用层迁移

2026-10-16从`D:\.workspace\LVGL_PC_GJOBS\src`迁入measurement、instrument和fonts，
保留各目录相对引用。源PC核心为10.0.0-dev，目标使用工程内9.6.0；不复制SDL、PC入口、
PC FreeRTOS端口或`pc_storage.c`。字体许可保留在fonts/OFL.txt。

## 当前运行链路

`vGUI_Task → ST7796_Init → lv_init → HAL_GetTick → GUI_DisplayInit → GUI_AppInit`

只有GUI任务访问LVGL。Key任务通过静态FreeRTOS队列投递键号，保留原蜂鸣器/LED功能：

- KEY1：上一主页面；KEY2：下一主页面。
- KEY3：测量页开始；标定页推进模拟标定向导；记录页打开第一条/返回；设置页进入测量参数/切换模式。
- 数值键盘、其他设置和确认弹窗的完整操作需要接入触摸。当前按键没有替代完整触摸交互。

显示采用480×320 RGB565、480×20（19,200字节）单局部缓冲与阻塞SPI。
驱动检查矩形和源步幅，逐像素按高字节在前发送；所有发送返回后才flush_ready。
显示缓冲位于普通SRAM。SPI1递归互斥锁覆盖完整LCD事务，Flash后端必须使用同一把锁。
初始化参数MADCTL=0x28与INVON保持原模组实测值；SPI频率仍为5.25MHz。

## 模拟与存储边界

gui_demo.c只包含移出的模拟采集/拟合/标定夹具，没有SDL；结果始终带simulated标志，
不能用来说明相机或几何测量算法已经实测。MaixCAM2后续通过MeasurementBackend接入。

GUI_AppInit(NULL)明确采用仅内存模式：设置、标定和64条历史在重启后丢失。
UI不把内存更新显示成Flash保存成功。GUI_StorageOps为后续板载存储提供load/save接口，
成功保存必须代表后端实际完成持久化；失败时UI恢复事务前模型。

GUI_RegisterTouch提供LVGL指针设备接口。读回调必须返回已旋转的480×320坐标，
读取失败或越界自动按松手处理。尚未启用FT6336U硬件；不能据此声称触摸已经完成。

## 内存与诊断

LVGL池48KiB；FreeRTOS堆24KiB；GUI任务栈2048 words=8KiB。
整份模型回滚快照放在GUI静态对象中，避免回调中多个1920B局部副本。
使用newlib-nano的_printf_float支持C snprintf，区别于LVGL自己的浮点配置。

调试观察`g_gui`（ready、断言位置、GUI剩余栈words、两套堆余量）及
`g_gui_display`（刷新次数、SPI错误、刷新耗时）。这些值须由实机运行取得。

## 构建与主机验证

从工程根目录运行现有预设：

```powershell
cmake --preset debug-armgcc
cmake --build --preset build-debug
```

本轮固件证据位于build/GUI_Migration；主机测试位于tests/gui_migration，
使用同一份LVGL9.6.0和48KiB配置，覆盖四页创建/重绘、模拟标定和测量、
保存失败回滚、RGB565顺序/步幅/边界和SPI错误收尾。
主机测试不代表ARM硬件、栈余量、触摸、DMA或Flash实机验收。

## CubeMX交接

用户已确认SPI1已配置、DMA未配置；本轮暂停DMA启用，等待用户生成代码。
SPI1_TX建议DMA2 Stream3 Channel3、Normal、Memory-to-Peripheral、
外设不递增/内存递增、Byte/Byte、FIFO关闭，DMA中断抢占优先级5。
DMA回调和异步刷新将在核验生成代码后接入；当前仍走阻塞SPI。
触摸RST/INT未分配，Flash MISO尚未在当前磁盘.ioc看到PA6；均需独立核实。
