# STM32 LVGL应用层迁移

2026-10-16从`D:\.workspace\LVGL_PC_GJOBS\src`迁入measurement、instrument和fonts，
保留各目录相对引用。源PC核心为10.0.0-dev，目标使用工程内9.6.0；不复制SDL、PC入口、
PC FreeRTOS端口或`pc_storage.c`。字体许可保留在fonts/OFL.txt。

## 当前运行链路

`main先完成OLED启动 → 创建I2C2总线锁/其他RTOS对象 → 启动调度器 → vGUI_Task → ST7796_Init → lv_init → HAL_GetTick → GUI_DisplayInit → FT6336U_Init → GUI_RegisterTouch → GUI_AppInit`

只有GUI任务访问LVGL。Key任务通过静态FreeRTOS队列投递键号，保留原蜂鸣器/LED功能：

- KEY1：上一主页面；KEY2：下一主页面。
- KEY3：测量页开始；标定页推进模拟标定向导；记录页打开第一条/返回；设置页进入测量参数/切换模式。
- 已接入FT6336U单指触摸，数值键盘、其他设置和确认弹窗由LVGL接收点击；实际功能待下载验收。

显示采用480×320 RGB565、480×20（19,200字节）单局部缓冲与SPI DMA分行发送。
每行最多480像素/960字节，逐像素按高字节在前发送并等待DMA完成；整个区域返回后才flush_ready。
显示缓冲位于普通SRAM。SPI1递归互斥锁覆盖完整LCD事务，Flash后端必须使用同一把锁。
初始化参数MADCTL=0x28与INVON保持原模组实测值；用户配置SPI /2，对应42MHz，长期稳定性未新增验收。

## 模拟与存储边界

gui_demo.c只包含移出的模拟采集/拟合/标定夹具，没有SDL；结果始终带simulated标志，
不能用来说明相机或几何测量算法已经实测。MaixCAM2后续通过MeasurementBackend接入。

GUI_AppInit(NULL)明确采用仅内存模式：设置、标定和64条历史在重启后丢失。
UI不把内存更新显示成Flash保存成功。GUI_StorageOps为后续板载存储提供load/save接口，
成功保存必须代表后端实际完成持久化；失败时UI恢复事务前模型。

GUI_RegisterTouch已接FT6336U硬件读回调，约每33ms由唯一GUI任务读取一次，
输出480×320横屏坐标。读取失败或越界自动按松手处理，并保留最后有效位置。
该周期为LVGL定时器配置，实际响应还受绘图/传输/调度影响，不能当成实测响应时间。

## H6触摸驱动（2026-10-17代码/编译通过待实测）

- `User/Hardware/FT6336U.c/.h`：PE1拉低20ms后释放，再等待至少300ms；
  固定七位地址0x38（HAL传入0x70），读取0xA3芯片ID，预期0x64；读取固件/厂商ID，
  设置普通工作模式及G_MODE=0轮询模式。时序依据[FocalTech手册](https://files.waveshare.com/wiki/common/FT6336U_Datasheet.pdf)，
  点数/事件/坐标格式交叉参考[Espressif官方驱动](https://github.com/espressif/esp-iot-solution/blob/master/components/display/lcd_touch/esp_lcd_touch_ft6336u/esp_lcd_touch_ft6336u.c)。
- 连续读0x02～0x06，解析点数、按下/持续接触/抬起及12位坐标；本工程只使用单指，
  双指时上报松手。通信超时/非法数据同样松手；连续3次通信失败后每秒尝试重新配置，
  不循环复位或重置共用I²C外设。若SDA/SCL被硬件持续拉低，仍需排查实物故障。
- `User/Hardware/I2C2_Bus.c/.h`：main先完成OLED启动检测、HAL延时及刷新，
  再创建静态互斥锁和其他RTOS对象并启动调度器。调度前只有main，通信不需要锁，
  也不要求提前创建锁；任务期OLED每条命令/每页数据与触摸每次传输共用同一把锁。
  锁只覆盖HAL事务和错误码读取；复位延时、整个OLED_Update均不持锁。
  取锁等待约25ms、触摸HAL传输超时20ms，均为配置值而非实测耗时。
- 原始范围X=0～319、Y=0～479，先交换X/Y，再反转屏幕Y，输出480×320坐标；
  2026-10-17用户通过青色光标确认“点上面出现在下面、点下面出现在上面”，据此
  固定`SWAP_XY=1、INVERT_X=0、INVERT_Y=1`，最终`x=raw_y、y=319-raw_x`。
  反转的是交换后的屏幕Y，不是原始触摸Y；四角端点已加入主机回归，实际点击仍待下载验收。
  原始范围不符时依据实测调整`RAW_X/Y_MIN/MAX`；尚未实现/声称自动标定或标定持久化。

当前修复版已关闭`FT6336U_OLED_DIAGNOSTICS=0`和`GUI_TOUCH_TRACE=0`，恢复OLED原内容，
不再创建临时圆点；调试计数与可选诊断代码保留，便于今后排查。ARM原build-debug实际编译链接退出0，
build/Debug/h6_touch_y_invert_fix_build.log无编译warning/error，RAM105328B/Flash753084B/CCM0，
ELF无touch_cursor/show_touch_diagnostics符号。驱动四角映射/松手/错误收尾主机1/1通过，
build/H6_TouchTest/h6_touch_y_invert_fix_test.log；关闭圆点后的真实LVGL页面输入测试1/1通过，
build/GUI_HostTest/h6_touch_y_invert_fix_test.log。未下载或取得修复后实机通过反馈。

以下保留修复前诊断过程，宏开启和等待数据的表述均为历史：

此前H6.5漏点诊断：用户已反馈显示和按键正常、XY/N/P正常，但底部页面切换同样无响应；
此前反复乱点偶尔响应，长按同一按钮2秒也难响应。原始数值/完整点击过程尚未取得。
启动故障按用户反馈已恢复，触摸尚未通过。已启用FT6336U.h中的临时开关
`FT6336U_OLED_DIAGNOSTICS=1`，在OLED上方48像素显示现有触摸数据，底部原按键提示保留。
此版不改I²C速率、采样周期、坐标方向/端点、灵敏度或原互斥行为，属于采证诊断版。
OLED只读取GUI任务留下的变量，不另查触摸、不额外增加OLED整帧传输次数，也不跨任务调用LVGL。

| OLED字段 | 读法 |
|---|---|
| R、XY | 触摸芯片原始坐标、换算后的横屏坐标；松手/失败时可能保留最后一点 |
| N、P | 当前手指数和已接受的按下状态；单指应N1/P1，越界会N1/P0并增加OUT |
| S、OK、ER | HAL状态（0正常/1错/2忙/3超时）、成功帧数和累计通信失败次数 |
| OUT、ID、RD | 原始坐标越界计数、芯片身份（预期64十六进制）、通信配置就绪（预期1） |
| LP、LR、LC | LVGL输入设备实际产生的按下/松手/点击次数，各显示最低3位；不等同驱动P |

OK/ER显示最低6位，OUT最低5位；不是整帧一致性快照。DT/EV继续保存在g_ft6336u，
当前OLED最后一行改为LP/LR/LC，避免继续重复只看驱动数据。
本轮先下载输入追踪版，点击并松开底部第二个页面按钮一次，观察青色小圆点是否跟手且松手消失，
记录LP/LR/LC各自是否加1（或提供OLED最后一行）。光标由LVGL自身移动且不可点击，不拦截按钮。
位置和驱动P正常还不能证明已命中按钮及完成点击；依此区分输入状态、命中与松手/取消过程。
未取得LVGL事件数据前不认定是哪一种原因，不盲改INVERT/SWAP/端点/周期。
排障后关闭FT6336U_OLED_DIAGNOSTICS恢复OLED、关闭lvgl_port.h的GUI_TOUCH_TRACE去掉临时光标。
诊断版ARM RAM105296B/Flash753708B，CCM0，日志`build/Debug/h6_touch_diagnostics_build.log`退出0，
未检出编译warning/error；扩展主机测试1/1通过（含75ms真实时间差与独立越界计数检查），
日志`build/H6_TouchTest/h6_touch_diagnostics_test.log`。诊断新增函数静态栈16B，无printf/局部大缓冲，
实际任务栈余量/触摸数据仍待实机；本轮未下载。

2026-10-17新增LVGL输入链路验证：`tests/gui_migration/gui_touch_test.c`编译实际lvgl_port、
LVGL9.6及四页面代码，注入按下/松手状态并运行真实LVGL定时器，不直接发送CLICKED；
四个底部导航及同一按钮内移动12像素后释放均可切页，追踪计数4次完整点击为4/4/4、
加入移动后LC=5。新目标gui_touch单独1/1通过，日志build/GUI_HostTest/h6_gui_touch_trace_test.log；
旧迁移/底层SPI测试未重跑，不称全部3项通过。该证据支持当前代码模拟输入可命中/切页，
尚未复现实机无响应，不能直接判断硬件时序、瞬态坐标抖动或具体事件丢失原因。
LVGL适配新增g_gui_touch的读取/上报及真实事件计数，显式绑定默认显示设备，
按下时用lv_indev_set_cursor显示不可点击10像素青色圆点，松手隐藏；事件监听只计数不改事件。
ARM实际编译链接退出0，build/Debug/h6_lvgl_touch_trace_build.log无编译warning/error，
RAM105328B/Flash754708B、CCM0；额外LVGL光标/监听对象从原48KiB池分配，真实余量仍待实机。

下载最终方向修正固件后按以下条件验收，不需要再次生成CubeMX：

1. 单指点击四页入口、测量按钮和数值输入框，检查对应控件响应。
2. 在屏幕四角附近触摸、拖动再松手，检查无轴交换/镜像、无松手后持续按住；同时确认OLED仍更新、按键/蜂鸣器能响应。
3. 无响应时在调试器展开`g_ft6336u`，提供下表关键值。若方向反了，描述左右反/上下反并提供原始坐标，按实测调整映射。

| 观察项 | 含义/预期 |
|---|---|
| ready、chip_id | 通信初始化后ready=1、chip_id=0x64；这不代表方向/交互已经实测通过 |
| read_count、press_count | 正常采样时read_count增长，每次重新按下press_count增长 |
| raw_x/raw_y、x/y | 原始触摸位置与横屏位置，用来区分通信问题和方向问题 |
| pressed、point_count | 单指按下pressed=1；松手、双指或错误时pressed=0 |
| last_status、last_i2c_error | 正常为0；status=1错误、2忙/锁等待失败、3超时；I²C错误位在释放锁前保存 |
| error_count、invalid_count | 累计通信失败和非法数据；正常操作应不持续增加 |

本轮ARM构建RAM105280B（80.32%）、Flash752628B（71.78%），CCM0，
日志`build/Debug/h6_touch_build.log`未检出编译warning/error。
`tests/touch_driver`以HAL/RTOS替身编译实际驱动，通过复位时序、总线锁、ID、四角映射、
事件/双指/非法数据、错误松手及恢复测试（1/1）；日志`build/H6_TouchTest/test.log`。
这些证据不能代替真实地址应答、触摸位置、OLED共存及完整UI实机验收。

2026-10-17启动故障修复：用户报告停在HAL_Delay的while。上一版在OLED_Init前
创建RTOS锁，触发本地FreeRTOS Cortex-M4端口的启动前临界区行为：
uxCriticalNesting初值0xaaaaaaaa，退出创建过程的临界区后BASEPRI仍保持0x50，
屏蔽优先级15的TIM6中断，uwTick停止，OLED的HAL_Delay(100)无法结束。
现已把建锁移到OLED全部启动操作后；Bus_Lock在调度前且非中断时允许不建锁通信，
运行任务后仍必须有锁。未改HAL_Delay、TIM6优先级或FreeRTOS内核。
新增回归检查在修复前失败、修复后1/1通过，日志分别为
`build/H6_TouchTest/h6_delay_before_fix_test.log`和`h6_delay_after_fix_test.log`。
ARM修复构建日志`build/Debug/h6_delay_startup_fix_build.log`，退出0、无编译warning/error，
占用仍为RAM105280B/Flash752628B。需停止旧调试会话，重新下载并复位，先确认能越过
OLED初始化；此后才续做H6.5触摸验收。源码因果链已查实，真实BASEPRI/uwTick值及恢复效果待用户复测。

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

SPI1_TX已配置DMA2 Stream3 Channel3、Normal、Memory-to-Peripheral、
外设不递增/内存递增、Byte/Byte、FIFO关闭，DMA/SPI中断抢占优先级5。
DMA回调和完成等待已接入；显示逐行发送并等待，不是绘图与发送并行的双缓冲。
2026-10-17已按用户授权同步.ioc与初始化：PA6=SPI1_MISO、
PE1=TP_RST（推挽输出，初始低保持触摸复位）、PE2=TP_INT（普通输入，未开EXTI）；
触摸SCL/SDA共用OLED的I²C2 PB10/PB11，100kHz。35002原理图有两颗10kΩ上拉到3.3V。
用户已确认完成触摸接线；触摸复位释放、共享总线互斥和FT6336U/LVGL输入已实现并编译。
本轮仅在main的USER CODE区增加总线初始化，未修改.ioc或生成区；实际触摸和LCD/Flash读回仍待实测。
