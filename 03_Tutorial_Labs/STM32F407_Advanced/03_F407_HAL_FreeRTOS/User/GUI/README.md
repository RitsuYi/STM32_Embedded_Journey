# 智能视觉测量仪 LVGL 应用层

当前入口为 H7.1 局部布局与结果可读性优化版（2026-10-19）。用户已认可整体布局/颜色；本轮修深色尺寸标注、完整胶囊、文字对齐、质检间距/提示及结果停留，通过主机/ARM验证，尚未下载。原H6/H7验收保持，新增实物效果待确认。

## 当前界面与操作

- 左下切换“测量 / 质检”，中键普通测量或自动质检启停，右下打开设置；三个触摸目标一直覆盖到Y319。
- 质检右侧包含标准设定、当前结论、累计统计、两条最近检测记录。“查看更多”及小箭头打开可滑动历史。
- 设置从右侧180ms滑入，内含Wi-Fi、设备信息、测量参数、显示、目标与上下公差、历史；列表可以手指拖动。数字键盘底行五键继续覆盖到Y319。
- KEY1切换模式（空闲且无菜单时）；KEY2设置/返回；KEY3主动作。原蜂鸣器和LED任务保持。
- 标准以0.01mm整数比较，区间包含两端。历史保留检测当时结论，修改标准不会重判；累计统计不因64条记录淘汰而减少。
- 自动质检完成后至少停留2.5秒供阅读；下一轮采集/拟合仍显示上次有效数值与结论，新结果完成才替换。首轮仍显示待检测，超时/取消不计数，停止按钮可随时响应。
- 尺寸标注采用深蓝灰0x49627A；数字/名称/单位和残差/状态按字体基线对齐。启停字14px粗体、等待提示灰色；质检右栏均衡两列，记录行距20px。合格率环内100%省略“.0”，非整数保留一位小数。

`GUI_AppInit → MeasurementPage + DashboardUI → MeasurementBackend` 是当前运行链路。`instrument_ui.c`旧四页源码保留作历史，不再从应用入口创建；界面均由唯一GUI任务访问。

## 二维皮肤和数据边界

参考图布局和二维圆环保留。蓝色0x98CBFA、正文0x78899B，底色0xFAF9F6略暖；正文12px、标题14px、主标题/质检数字16px、主尺寸数字26px，官方原尺寸取字。底部为36px高完整胶囊（Y276～311，下方留阴影，触摸到Y319），独立16px轻轮廓盾牌/直尺/齿轮，播放/停止略小，16px图标+8px间距+实际文字作为一组居中；保留四标题的完整16px图标。Wi-Fi始终蓝色，未连接带斜杠，竖线分隔时间。按钮按下/禁用遮罩沿用胶囊圆角，键盘特殊键样式保持。见`fonts/README.md`和`measurement/ui_icons.h`。

`assets/ui_skin.c`以1440B共享行缓存生成292/278×232原尺寸RGB888背景。圆环保留ARGB8888颜色/透明度并常驻Flash。19200B单局部缓冲内部RGB888（全宽13行），让卡片、按钮、进度、字体边缘及透明缩放先混合，再由`lvgl_port.c`按屏幕绝对坐标8×8固定抖色，原地压缩成RGB565。屏幕仍每像素2字节，纯白输出0xFFFF；没有整屏缓存。原ST7796窗口/stride/高字节在前、960B逐行DMA等待、单GUI/软件绘图单元保持，未改硬件/触摸/时钟/液晶寄存器。旧20行是绘图缓冲，不是一次DMA20行。有限色阶及面板响应仍需实机核验。

当前仍是模拟相机（包括32.00×20.00mm矩形像素角点→固定映射→尺寸路径），不是MaixCAM2真实测量。`gui_camera_config.h`集中放固定参数，目前仅适配模拟夹具；真实安装需一次性替换。界面没有现场标定向导。

`GUI_AppInit(NULL)`为仅内存存储，重启清空；没有RTC时显示开机时间，不冒充参考图日期。`GUI_AppSetNetwork`预留非阻塞连接/断开请求，true只表示受理，只有`GUI_AppNetworkStatus`后端回报才变为已连接。两接口必须在GUI任务调用，其他任务用队列转交。网络和Flash均未接硬件；存储模型布局新增了公差/结论/时间/统计，后续Flash后端必须做版本化，不能原样载入旧结构。

## 当前验证

- 原预设：`cmake --build --preset build-debug`。最新日志`build/Debug/h7_1_layout_polish_build.log`：RAM112440B/131072B（85.78%，不变），Flash875952B（83.54%），CCM0，无需外部Flash。C编译通过，仍有LOAD段RWX链接告警。
- `build/H7_1_Host/layout_polish_test.log`：真实LVGL及实际ST7796驱动契约2/2通过。新增2.5秒阅读期限、下一轮采集的结果区域逐像素保持、按钮上下弧线和环内百分比宽度；两布局1/7/13/20行及裁剪逐像素一致、渐变/纯色/字体/原回归保持。52KiB主机池峰41568B/free17464B，不能代替实机水位。48KiB曾在切菜单变换时连续分配失败，现52KiB池及12KiB简单图层预算回归通过。
- 覆盖模式、底边真实指针输入、自动启停/超时、公差边界与旧结论、失败回滚、数字键盘、设置/历史拖动、Wi-Fi请求与状态分离、缩放、矩形和64条容量。
- `build/H7_1_Host/measurement_layout_polish.png`、`inspection_idle_layout_polish.png`、`inspection_pass_layout_polish.png`及`inspection_acquiring_layout_polish.png`为本版实际主机渲染，另有超差/矩形。旧`*_render_audit.png`等保留历史。20行RGB888测试缓冲及整屏截图仅在电脑存在。高精度混合/抖色未变，未新增实机性能结果；性能方案见主进度5.10，不照旧20行565预算直接实施。

首次建立主机目标（MinGW GCC + Ninja）：

```powershell
cmake -S tests/ui_dashboard -B build/H7_1_Host -G Ninja -DCMAKE_C_COMPILER=E:/DevEnv/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=E:/DevEnv/mingw64/bin/g++.exe -DCMAKE_BUILD_TYPE=Debug
cmake --build build/H7_1_Host --target ui_dashboard_test
ctest --test-dir build/H7_1_Host --output-on-failure
```

当前步骤/验收依据统一维护于根`PROJECT_PROGRESS.md`，下面仅保留迁移与触摸历史，旧按钮数量和KEY操作不代表当前H7.1界面。

---

# STM32 LVGL应用层迁移

2026-10-16从`D:\.workspace\LVGL_PC_GJOBS\src`迁入measurement、instrument和fonts，
保留各目录相对引用。源PC核心为10.0.0-dev，目标使用工程内9.6.0；不复制SDL、PC入口、
PC FreeRTOS端口或`pc_storage.c`。字体许可保留在fonts/OFL.txt。

## 当前运行链路

2026-10-18补充：数字键盘底行+/−、0、.、左、右下方新增到Y319的命中区，原键盘位置/外观及上三排不变。
原H6.5验收保持，此次补充ARM及真实GUI输入回归通过、尚未下载实测；调试显示/统计继续保持移除。

2026-10-18用户明确确认H6.5验收完成，H6触摸交互按当前方案收口，下一阶段为H7四页面整体验收。
已移除I²C调试条、OLED触摸数据、小蓝点、输入事件/采样/刷新统计、栈堆周期监测及SPI调试计数。
触摸只保留单指读取、方向转换、最后有效点和重试状态；OLED保留原启动内容和按键提示。
按钮命中扩展、总线互斥/自动恢复、DMA等待及必要错误保护保留。
清理版ARM及相关主机回归通过，未由Codex下载或亲测；详细证据/排障历史统一见PROJECT_PROGRESS.md。

此前已按用户要求撤销OLED停用/400kHz试验：恢复OLED开机初始化及刷新任务，
I²C2与`.ioc`恢复100kHz，移除该试验开关及条件编译；本轮继续保留商家触摸解析。
试验历史保留在PROJECT_PROGRESS.md，不将未确认的试验效果当作验收证据。

`main先完成OLED启动 → 创建I2C2总线锁/其他RTOS对象 → 启动调度器 → vGUI_Task → ST7796_Init → lv_init → HAL_GetTick → GUI_DisplayInit → FT6336U_Init → GUI_RegisterTouch → GUI_AppInit`

只有GUI任务访问LVGL。Key任务通过静态FreeRTOS队列投递键号，保留原蜂鸣器/LED功能：

- KEY1：上一主页面；KEY2：下一主页面。
- KEY3：测量页开始；标定页推进模拟标定向导；记录页打开第一条/返回；设置页进入测量参数/切换模式。
- 已接入FT6336U单指触摸，数值键盘、其他设置和确认弹窗由LVGL接收点击；H6.5交互按用户确认收口，H7完整业务另行验收。

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

GUI_RegisterTouch已接FT6336U硬件读回调，输入定时器目标15ms，由唯一GUI任务读取，
输出480×320横屏坐标。读取失败或越界自动按松手处理，并保留最后有效位置。
该周期为LVGL定时器配置，实际响应还受绘图/传输/调度影响，不能当成实测响应时间。

2026-10-18按用户要求扩大应用层按钮命中范围，外观和触摸坐标保持原样：

- 底部四键仍是101×25、Y284～308，左右/上方保留4像素容错，下方延伸到Y319。
  按钮归整屏root管理，坐标维持原位，避免旧33像素高导航背景截断底边的命中搜索。
- 开始测量、返回、操作、设置行、记录行和确认/取消等独立按钮，四周最多外扩8像素。
  同父容器的相邻按钮按空隙中线划分，密集记录行2像素间隔各分1像素；父容器仍限制范围。
  数字键盘的本体沿用LVGL矩阵判定；底行五键另补Y313～319，按实际键间隙中线分列，
  由原键盘处理键值，保留长按、同列偏移及异列取消。正负号仍遵守原输入框的允许字符/取值规则。
- 命中回调只回答“坐标是否在范围内”，由LVGL正常处理按下/松手、隐藏、禁用和弹窗遮挡。
  固定9.6版本需包含event_private头取得命中结果结构；键盘另仅读取buttonmatrix_private布局，未修改第三方库。
- 用户已确认H6.5验收完成；本轮清理版构建/软件回归证据见PROJECT_PROGRESS.md。

## H6触摸驱动（2026-10-18用户确认实机验收通过）

- `User/Hardware/FT6336U.c/.h`：PE1拉低20ms后释放，再等待至少300ms；
  固定七位地址0x38（HAL传入0x70），读取0xA3芯片ID并核对0x64，不再读取仅供调试的固件/厂商ID，
  保持硬复位后的默认工作模式，不额外写0x00/0xA4或扫描/灵敏度参数。
  主要依据用户提供的[商家ctp.c](<D:/.Data/spi 3.5寸屏幕资料/CL35BC1070-40A_CTP/测试程序/STM32F1_ST7796_IPS_4SPI_CTP Demo_V1/HARDWARE/TOUCH/ctp.c>)，
  并核对同包STC32_4SPI版本，初始化与CONTACT规则一致。
- 按商家顺序先读0x02点数1字节，再读0x03～0x06的坐标4字节，两次HAL事务共用一把锁。
  本工程只支持单指N1，多指松手；不扫第二槽，不再跨帧跟踪编号。
  **仅(XH & 0xF0)==0x80的CONTACT有效**，初始DOWN、UP均不更新界面位置。
  按商家忽略原始双零；单轴0仍合法，不裁边或把0移到按钮。
  坐标只做原有横屏交换/镜像，直接交给LVGL，不再做中值/边缘确认/短按补交/圆点余辉。
  通信超时/非法数据仍松手；连续3次通信失败后每秒尝试重新配置，
  不反复复位触摸芯片；共用I²C控制器的故障统一由下述总线层处理。
- `User/Hardware/I2C2_Bus.c/.h`：main先完成OLED启动检测、HAL延时及刷新，
  再创建静态互斥锁和其他RTOS对象并启动调度器。调度前只有main，通信不需要锁，
  也不要求提前创建锁；任务期OLED每条命令/每页数据与触摸每次传输共用同一把锁。
  锁覆盖HAL事务、错误码读取及必要的故障恢复；触摸芯片复位延时、整个OLED_Update不持锁。
  取锁等待约25ms、触摸HAL传输超时20ms，均为配置值而非实测耗时。
  2026-10-18补充：HAL传输返回忙/超时等故障时，每秒最多一次，在原锁内关闭/复位I²C2、
  开漏释放PB10/PB11，必要时补最多9个数据时钟、产生STOP，再恢复AF4和原100kHz配置。
  SCL持续低、SDA未释放或重新初始化失败均有限退出；不把失败帧重试成有效点击。
  单纯设备不应答、取锁失败不重置总线，启动调度器前不执行RTOS延时恢复。
  方法依据[NXP UM10204第3.1.16节](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)；
  补STOP时另有一次SCL上升，不能把它误计为第10个数据补时钟。
  OLED任务栈由128增至256个32位字（1024B），运行时FreeRTOS堆多用512B，静态堆容量不变。
- 共享总线故障模型及失败输出自检3/3通过，实际LVGL输入回归1/1通过；
  测试从返回值、坐标、按键操作及HAL/GPIO替身观察行为，不依赖正式固件的调试变量。
- 原始范围X=0～319、Y=0～479，先交换X/Y，再反转屏幕Y，输出480×320坐标；
  2026-10-17用户通过青色光标确认“点上面出现在下面、点下面出现在上面”，据此
  固定`SWAP_XY=1、INVERT_X=0、INVERT_Y=1`，最终`x=raw_y、y=319-raw_x`。
  反转的是交换后的屏幕Y，不是原始触摸Y；四角端点已加入主机回归，H6.5实际交互按用户确认收口。
  原始范围不符时依据实测调整`RAW_X/Y_MIN/MAX`；尚未实现/声称自动标定或标定持久化。

触摸排障、商家协议审计、历史诊断字段及每轮测试日志均保留在根目录PROJECT_PROGRESS.md；
其中旧调试宏和全局诊断变量已从当前固件删除，不再作为使用说明。

## 内存配置

当前LVGL池52KiB，简单图层分块上限12KiB；FreeRTOS堆24KiB；GUI任务栈2048 words=8KiB。下方旧证据中的48KiB为历史值。
整份模型回滚快照放在GUI静态对象中，避免回调中多个1920B局部副本。
使用newlib-nano的_printf_float支持C snprintf，区别于LVGL自己的浮点配置。

已移除周期栈堆监测与刷新耗时统计；LVGL分配断言及错误停机保护继续保留。

## 构建与主机验证

从工程根目录运行现有预设：

```powershell
cmake --preset debug-armgcc
cmake --build --preset build-debug
```

本轮ARM证据为`build/Debug/h6_debug_cleanup_build.log`及`h6_debug_cleanup_symbols.log`，
RAM105176B、Flash753540B、CCM0；调试显示/统计符号已移除，触摸、按钮扩展、总线恢复及DMA链路仍在。
`build/GUI_HostTest/h6_debug_cleanup_test.log`记录真实LVGL输入单独1/1通过；
`build/H6_TouchTest/h6_debug_cleanup_test.log`记录触摸/共享总线恢复/失败报告3/3通过。
测试观测器只编入主机测试程序。未重跑未修改的旧SPI套件，不用主机通过代替新固件实机结果。

本次键盘补充的最新日志为`build/Debug/h6_keyboard_bottom_build.log`，RAM105184B/Flash754364B/CCM0；
`build/GUI_HostTest/h6_keyboard_bottom_final_test.log`记录真实LVGL输入单独1/1通过，
涵盖五键从可见底边到Y319的每行、原键效果一致、分列边界/长按/滑动取消/禁用及原回归。
单键布局保持原样，无新调试对象；未下载或重跑无关驱动/旧SPI套件。

## CubeMX交接

SPI1_TX已配置DMA2 Stream3 Channel3、Normal、Memory-to-Peripheral、
外设不递增/内存递增、Byte/Byte、FIFO关闭，DMA/SPI中断抢占优先级5。
DMA回调和完成等待已接入；显示逐行发送并等待，不是绘图与发送并行的双缓冲。
2026-10-17已按用户授权同步.ioc与初始化：PA6=SPI1_MISO、
PE1=TP_RST（推挽输出，初始低保持触摸复位）、PE2=TP_INT（普通输入，未开EXTI）；
触摸SCL/SDA共用OLED的I²C2 PB10/PB11，100kHz。35002原理图有两颗10kΩ上拉到3.3V。
用户已确认触摸接线和H6.5实机交互验收；本轮清理仅涉及应用/驱动调试代码，未改.ioc或生成区。
LCD/Flash读回与持久化仍未验收，不因H6收口而自动升级。
