# PROJECT_PROGRESS — 基于STM32的智能视觉尺寸测量与质量检测系统设计

> **Codex长期实施计划与进度记录｜V3.1**  
> 更新日期：2026-10-16  
> **当前步骤：H5 — 用户配置SPI1_TX DMA；H4显示适配与H7四页面的阻塞SPI版本已编译通过，待实机验证**  
> **紧接下一步：核验用户生成的DMA配置、启动向量和构建，再接入异步SPI刷新。**  
> **下一个动作：用户在CubeMX为SPI1_TX添加DMA2 Stream3 Channel3（Normal/Byte），开启DMA与SPI1中断并设抢占优先级5；生成后回复“DMA配置已生成”。**  
> **教学安排：用户2026-10-16明确要求结束LVGL逐节课程，改为直接完成迁移；需要CubeMX操作时停下来由用户完成。课程结束不代表DMA/触摸/存储或实机验收已完成。**  
> 记录规则：已完成与待验证严格区分；不把“编译通过”当成“实机功能验证通过”。

---

## 0. 使用说明（跨对话续接请先看这里）

这是项目**主进度文件**，Codex 在本工程新会话开始时必须根据根目录 `AGENTS.md` 主动读取本文件。每次有用户确认的进展、代码/构建证据或新阻碍，Codex 需要**直接修改此固定文件名**，更新日期、当前步骤、实际状态、待办和日志；**不得删除未完成的长期阶段**。与本文件不同的客观源码或测试证据需要核实后同步记录。不要无意义地另建 V3.2/V3.3 文件；以 Git 历史和文内日志追踪。

**当前最重要的事实**：H3五色/四角矩形已有用户实机验收。按用户最新授权，已保留F407 LVGL v9.6.0并迁入PC四页面、状态机、模型和五份字体，接入Tick/局部缓冲/阻塞SPI刷新、模拟后端及按键消息队列。最新ARM固件编译链接通过，无编译警告/错误：RAM **104,480B/128KiB（79.71%）**，Flash **725,964B/1MiB（69.23%）**，CCM未使用。两项主机测试通过；**尚未下载或验证LVGL实机显示**。用户确认“SPI1已配置，DMA未配置”，现停在其CubeMX操作边界；触摸与Flash仅有平台接口，未接入硬件。

## 1. 固定项目基线（后续持续保留）

| 项目 | 约定方案 |
|---|---|
| 课题 | **基于STM32的智能视觉尺寸测量与质量检测系统设计** |
| 主控 | 嘉立创**天空星 STM32F407VGT6单独核心板**，**无筑基学习底板** |
| 任务定位 | 以STM32嵌入式软件为核心，面向平面工件：圆环、矩形，后期可扩展多孔板；测量、智能尺寸标注、公差PASS/FAIL、保存记录、误差分析 |
| 视觉前端 | **MaixCAM2**：后期使用MaixPy/OpenCV提取工件轮廓与图像特征，向STM32发送有效轮廓/测量信息 |
| UI | **LVGL**，PC端Windows+SDL2四页面仿真已完成；F407实机逐步复现；当前固定目标版本为**v9.6.0稳定标签**，不追开发分支 |
| 系统软件 | STM32 HAL + FreeRTOS + CMake + GNU Arm + Ninja；用户自己写驱动与任务 |
| LCD | 鸿讯电子35002 **14针SPI转接板** + CL35BC219-40A屏，ST7796/ST7796S；原生**320×480**，横屏逻辑**480×320** |
| 触摸 | FT6336U电容触摸（I²C，H6再接） |
| 存储 | 优先利用板载W25Q128 SPI Flash，使用前须协调与LCD共享的SPI1总线；也可后续按论文需求调整 |
| 供电 | LCD使用**外部稳压3.3V**，LCD电源GND与STM32数字GND**共地**；**不得直接并联两个3.3V电源正极** |
| 实施方法 | **2026-10-16用户更新授权**：直接迁移LVGL与PC四页面，结束逐节课程；涉及CubeMX配置由用户操作后核验；代码、编译、实机证据继续严格区分 |
| 毕设产出 | 系统实物、演示程序、通信和测量算法、标定与精度数据、约25页正文（以学校最终要求为准） |

### 软件结构约定

```text
03_F407_HAL_FreeRTOS/
├── Core/                  # CubeMX生成代码
├── Drivers/               # ST HAL/CMSIS
├── FreeRTOS/               # RTOS内核
├── User/
│   ├── Hardware/           # 2026-10-16核实的实际目录；沿用用户已有迁移
│   │   ├── ST7796.c
│   │   └── ST7796.h
│   │   # 另有SPI1_Bus.c/.h，供LCD及未来Flash事务互斥
│   ├── GUI/                # 四页面应用、平台适配、模拟后端、字体
│   │   ├── measurement/
│   │   ├── instrument/
│   │   ├── fonts/
│   │   └── lvgl_port.c     # Tick由GUI任务注册；本模块负责显示/输入接口
│   └── Task/
│       ├── GUI_Task.c
│       ├── GUI_Task.h
│       └── ...             # 原有按键/OLED/蜂鸣器任务
├── LVGL/                   # LVGL核心源码，不混入Windows仿真工程
├── lv_conf.h              # 当前工程独立的LVGL配置
├── CMakeLists.txt
└── CMakePresets.json
```

- `ST7796`模块：GPIO、SPI传输、复位、寄存器初始化、显示区域和像素写入。
- 历史记录曾称`User/Driver/`；本轮实际文件为`User/Hardware/ST7796.c/.h`，不为对齐旧文档而移动文件。
- `GUI_Task`模块：FreeRTOS任务、LCD和LVGL初始化及`lv_timer_handler()`运行；今后由此任务统一访问LVGL界面API。
- `main.c`：初始化外设、创建任务和启动调度器，不堆积LCD/UI实现。
- 当前用户选择：ST7796驱动内部可直接使用`vTaskDelay()`，**不使用延时回调函数指针**；`ST7796_Init()`使用`void`，SPI轮询使用`HAL_MAX_DELAY`。需注意`HAL_MAX_DELAY`不保证所有错误均“卡住”，将来工程验收时应补充异常检测。

## 2. 完整路线图与状态（任何更新都必须保留）

| 编号 | 任务 | 当前状态（2026-10-16） | 通过标准 / 下一条件 |
|---|---|---|---|
| **P0** | Windows SDL2 + LVGL 四页面UI仿真 | **已完成**（用户确认） | 电脑版四页面可运行；后续迁移应用层逻辑 |
| **H0** | 核心板、屏幕、转接板、引脚与资源核查 | **已完成** | 确认单核心板、35002接口、ST7796、FT6336U及板载Flash |
| **H1** | LCD供电及接线 | **实机验收通过**（显示功能；电源长期稳定性仍需观察） | 已完成LCD正常显示；继续注意独立3.3V、共地、异常发热/电源噪声 |
| **H2** | CubeMX SPI1、GPIO、HAL/CMake基础构建 | **已完成** | SPI1 5.25MHz，GPIO宏生成及ARM编译链接成功 |
| **H3.1** | SPI命令和数据发送 | **实机验收通过**（实际显示佐证） | DC低命令、DC高数据；底层SPI可写入屏幕 |
| **H3.2** | 硬复位和ST7796初始化 | **实机验收通过**（实际显示佐证） | 初始化后可正常显示；固定模组适配参数 |
| **H3.3** | RGB565五色纯色填充 | **实机验收通过**（用户确认） | 黑、白、红、绿、蓝全部正确 |
| **H3.4** | 横屏坐标与局部矩形绘制 | **实机验收通过**（用户确认） | 480×320四角矩形与中央色块正确 |
| **H4.1** | LVGL版本、`lv_conf.h`、CMake移植 | **已完成**（版本及构建配置验收；用户授权继续整体迁移） | 保留v9.6.0、RGB565/LV_OS_NONE/软件绘图；关闭examples/demos/ThorVG；完整四页面阶段池改为48KiB，证据见第5节 |
| **H4.2** | LVGL Tick、局部缓冲、SPI Flush | **配置/编译通过待实测** | HAL_GetTick、19,200B单缓冲、步幅/字节序及阻塞flush已接入；主机传输契约测试通过，颜色与屏幕刷新仍待实测 |
| **H4.3** | STM32上运行最小LVGL界面 | **配置/编译通过待实测**（用户取消独立课程，合并到H7） | 未另写临时标签/按钮程序；由迁入的完整四页面统一验收实机显示 |
| **H5** | SPI DMA分块刷新与性能优化 | **进行中**（用户CubeMX配置待完成；DMA尚未启用） | 待SPI1_TX DMA2 Stream3 Channel3及中断生成并核验，再实现异步刷新与完成通知；当前运行路径为阻塞SPI |
| **H6** | FT6336U I²C电容触摸 | **未开始**（LVGL输入注册接口已准备，硬件驱动未启用） | RST/INT及I²C共享条件待确认；GUI_RegisterTouch接受已旋转坐标，不能当作FT6336U实机已完成 |
| **H7** | FreeRTOS整合PC端四页面UI | **配置/编译通过待实测** | 应用/字体/模拟业务已迁入，主机测试通过；实机四页、完整触摸交互与资源余量待验证；持久化后端未接入 |
| **A1** | STM32几何拟合、尺寸标注和公差判定 | **未开始** | 圆环、矩形模拟轮廓计算尺寸；上下公差、PASS/FAIL、提示和记录 |
| **C1** | UART DMA通信协议、PC模拟视觉输入 | **未开始** | 串口帧同步、校验、超时、缓存/队列；模拟轮廓可驱动测量和UI |
| **V1** | MaixCAM2实际视觉算法 | **未开始** | 相机提取圆环/矩形轮廓，处理基本误检并发送STM32 |
| **V2** | 像素-毫米标定、精度和重复性测试 | **未开始** | 实际工件测量、误差统计、重复性/可靠性实验 |
| **D1** | 全系统测试、现场展示与毕业论文 | **未开始** | 演示自动测量→标注→公差判断→保存，形成合规论文和实验数据 |

## 3. LCD硬件接线与板载资源（已由显示测试基本验证）

面板正面朝自己、14针排针在上方：**右端1 VCC，左端14 INT**。这是35002转接板14针，不是原始液晶40针。

| 35002针脚 | 信号 | 当前连接 / 处理 |
|---|---|---|
| 1 | VCC | 外部稳压3.3V正极 |
| 2 | GND | 外部电源GND与F407 GND共地 |
| 3 | LCD_CS | **PE0** GPIO，低有效 |
| 4 | LCD_RST | **PB1** GPIO，低有效 |
| 5 | LCD_DC | **PB0** GPIO，命令0/数据1 |
| 6 | LCD_MOSI | **PA7 / SPI1_MOSI** AF5 |
| 7 | LCD_SCK | **PA5 / SPI1_SCK** AF5 |
| 8 | LCD_BL | **PB14** GPIO，背光高有效（35002模块接口） |
| 9 | LCD_MISO | 首次只写测试未使用 |
| 10 | FT6336U SCL | H6待接 |
| 11 | FT6336U RST | H6待分配GPIO |
| 12 | FT6336U SDA | H6待接 |
| 13 | NC | 不接 |
| 14 | FT6336U INT | H6待分配GPIO |

**当前SPI1配置**：Master，`.ioc`与`Core/Src/spi.c`实际为`SPI_DIRECTION_2LINES`；当前仅配置SCK/MOSI并进行发送，MISO未验证（修正旧文档“Transmit Only”的表述，本轮未改外设）。8位；MSB First；Mode 0（CPOL Low、CPHA 1 Edge）；软件NSS；APB2 84MHz / 16 = **5.25MHz**。PA4作为板载W25Q128 `FLASH_CS`保持高电平。SPI1总线与Flash共享时将来需严格同步访问并处理可能不同的SPI模式；不能让两个任务同时直接操作。

**已有其他外设资源（以后分配不能覆盖）**：PB2=LED1，PB6=LED3，PC8=BUZZER，PE13/14/15=KEY3/2/1，PB10/PB11=I²C2（现有OLED），PA9/PA10=USART1，PA13/PA14=SWD。PB6已被LED3占用，故**不能直接把I²C1 PB6/PB7当作空闲引脚**；触摸H6需重新评估现有I²C2与触摸共总线可行性及地址冲突、上下拉/总线负载。板载TF、USB、Flash资源同样要复核，避免照搬其他F407开发板方案。

## 4. ST7796已通过的驱动实验（H3验收证据）

- 初期只有背光/雪花屏，后续通过独立测试验证了SPI显存写入通路。
- 颜色异常的排查记录：未设置反色时，红色曾显示为蓝色或黄色；在`MADCTL=0x20`并开启`INVON(0x21)`后呈现黑/白/蓝/绿/红（红蓝交换）；最终采用**MADCTL参数`0x28` + 命令`INVON 0x21`**，黑白红绿蓝顺序全部正确。
- 目前模组验证成功的最小初始化顺序：**硬复位 → 0x11 SLPOUT → 0x3A/0x55 RGB565 → 0x36/0x28横屏+BGR → 0x21 INVON → 0x29 DISPON → 开背光**。上述组合是**本模组实测适配值**，并非可以通用于所有ST7796面板的固定参数。
- `ST7796_SetAddressWindow()`设置`0x2A`列和`0x2B`行；`RAMWR 0x2C`连续发送RGB565像素；`ST7796_FillScreen()`以及`ST7796_FillRect()`采用小块缓冲与边界裁剪。
- 用户已报告**四角不同颜色矩形及中间白色矩形都显示正确**：H3.4通过。
- 后续LVGL Flush**不能简单复用纯色`FillRect()`**（它只生成统一颜色）；需提供“根据传入像素缓冲区原样发送一个矩形区域”的独立底层接口，并注意RGB565**字节顺序**。

### 尚需观察的硬件问题

用户曾报告STM32核心板附近有非常轻微的滋滋声，且未启动LCD时也有。尚未确定具体来源；可能是供电电感/电容或外接蜂鸣器，并**不能无证据认定为正常或损坏**。如果声音明显变大、供电异常、芯片发热或反复复位，应断电检查。当前项目实物显示功能已正常运行，此事为非阻断但需继续观察的风险。

## 5. 当前软件迁移结果与H5配置交接

### 5.1 2026-10-16 用户更新范围及实际实施

用户明确要求跳过逐节步骤、直接按方案迁移并结束LVGL课程；同时要求需要CubeMX时停下来交给用户。该最新授权取代旧的“一次只指导一步”实施节奏，保留全部阶段和验收边界。

- **驱动与显示**：新增`ST7796_WriteRGB565()`，接收原生小端RGB565及源步幅；检查边界，逐像素高字节先发，命令/数据发送返回值逐次检查，错误时拉高CS并解锁。新增SPI1递归互斥模块，既有驱动接口也按事务加锁。沿用MADCTL=0x28、INVON及5.25MHz，无引脚更改。
- **LVGL适配**：`User/GUI/lvgl_port.c/.h`创建480×320显示对象与480×20/19,200B单缓冲；取活动缓冲实际stride，同步传输返回后flush_ready，错误计数可调试观察。缓冲链接地址**0x200016F4**，位于DMA可访问的普通SRAM。本轮未实现/启用DMA。
- **应用层**：从`D:\.workspace\LVGL_PC_GJOBS\src`迁入measurement、instrument及fonts，保留相对目录和字体许可；源核心10.0.0-dev，目标仍为9.6.0。仅改目标副本，源PC工程未改；不迁SDL、PC主入口、PC FreeRTOS端口或fopen存储。
- **业务与交互**：`gui_demo.c`迁出模拟测量/标定逻辑，使用MeasurementBackend及InstrumentServices，结果保留simulated标志，不能作为MaixCAM2/实测算法证据。`gui_app.c`组装四页；KEY1上页、KEY2下页、KEY3当前主动作，原LED/蜂鸣器动作保留。按键经静态队列进入唯一GUI任务；数值编辑等完整交互仍待触摸。
- **输入和存储边界**：GUI_RegisterTouch是指针设备读回调接口，未接FT6336U硬件；GUI_StorageOps是load/save接口，未接W25Q128持久化。目前GUI_AppInit(NULL)，UI明确显示“模拟测量/仅内存”，设置、标定、记录重启后丢失，不伪报Flash保存成功；接入后端若保存失败会回滚模型。
- **内存与兼容修复**：LVGL池初试64KiB时普通SRAM链接占用120,864B（92.21%）；依据主机压力测试改为**48KiB**，保留更多普通SRAM。FreeRTOS堆改为24KiB，GUI栈2048 words=8KiB；多个1920B局部模型副本改成GUI静态回滚快照，初始化避免大复合字面量占栈。整数格式改用PRIu32，C snprintf浮点输出显式链接_printf_float；LVGL自身LV_USE_FLOAT仍为0。启用分配/空指针断言和GUI栈水位诊断。
- **可观察量**：`g_gui`记录初始化、断言文件/行、GUI剩余栈words、FreeRTOS及LVGL堆余量；`g_gui_display`记录刷新/SPI错误/耗时。这些运行值尚未从实机取得。ARM .su中用户应用最大单函数静态栈为instrument_calculate_calibration **1232B**，不代表整条调用链或实机栈已验收。

### 5.2 本轮验证证据

1. **ARM Debug编译链接通过**：目录`build/GUI_Migration/`，日志`configure.log`、`build.log`；GNU Arm GCC10.3.1+CMake4.2.3+Ninja，最后构建未发现编译warning/error。RAM **104,480B/128KiB（79.71%）**、Flash **725,964B/1MiB（69.23%）**、CCM0；普通SRAM链接余量26,592B，包含24KiB FreeRTOS堆和48KiB LVGL池的静态预留，不等于运行时空闲堆。
2. **主机测试2/2通过**：`tests/gui_migration/`，使用MinGW GCC13.1和同一LVGL9.6.0/48KiB配置；日志`build/GUI_HostTest/test.log`与`Testing/Temporary/LastTest.log`。覆盖四页创建、重复切页/重绘、模拟标定、圆环/矩形测量、仅内存记录、持久化失败回滚、新增提示字形。
3. **主机池压力数据**：max_used **38,656B**、最终free **5,912B**、碎片14%、累计1415次flush。主机是64位指针环境，此数据用于发现应用分配问题，不能充当F407运行内存实测。
4. **底层契约测试**：直接编译实际ST7796.c并以HAL桩验证窗口端点、RGB565字节序、带padding行步幅、超过256像素的分块、非法区域拒绝、逐次传输出错后CS/锁正确收尾。仅证明软件契约，未证明LCD收到数据。
5. **未验证项**：未下载/上电跑LVGL；屏幕颜色、持续刷新、任务真实栈余量/峰值、SPI速度稳定性待实测。DMA、FT6336U、Flash持久化尚未实现或启用，不因课程结束标记为实机通过。

### 5.3 当前CubeMX动作与停等条件

用户回复：**“SPI1已配置，DMA未配置，请指导我完成配置”**。因此停在H5硬件配置交接，不直接手改.ioc或生成区来代替用户。

- 在当前`.ioc`的SPI1 → DMA Settings添加SPI1_TX，选择**DMA2 Stream3 Channel3**；Memory-to-Peripheral、Normal、Peripheral Increment Disabled、Memory Increment Enabled、Byte/Byte、Low、FIFO Disabled。
- NVIC开启**DMA2 Stream3 global interrupt**和**SPI1 global interrupt**，抢占优先级均为**5**、子优先级0；本工程configMAX_SYSCALL_INTERRUPT_PRIORITY=(5<<4)，后续ISR通知必须遵守此阈值。
- 保持现有SPI模式、8位、5.25MHz与LCD引脚；保留手工FreeRTOS，不新增CubeMX第二套RTOS。代码生成启用保留USER CODE；禁止自动批量删除旧文件。
- 生成后核验`hdma_spi1_tx`、DMA_CHANNEL_3、__HAL_LINKDMA、MX_DMA_Init在SPI初始化前、DMA2_Stream3_IRQHandler及SPI1_IRQHandler、Core/Src/dma.c纳入构建。
- 特别核验启动向量仍路由vPortSVCHandler/xPortPendSVHandler/xPortSysTickHandler，避免CubeMX重生成后调度失效。本轮已将启动文件、main.c、FreeRTOSConfig.h、根CMake和.ioc逐文件备份在`build/GUI_Migration/pre_cubemx/`，未删除文件。
- 用户生成代码并明确回复后再接DMA；触摸RST/INT、I²C共享以及Flash的MISO/后端另行确认。当前磁盘.ioc未见PA6 SPI1_MISO，用户“SPI1已配置”不自动等于Flash读取已验证。

### 5.4 H4.1配置收口历史（以下保留原验证过程）

### 已有事实

1. 曾将PC端Windows仿真仓库整个复制进F407项目，其中**外层目录**含Windows示例、SDL/FreeRTOS、tests/tools等，不适合整体移植；后来已选择单独的LVGL核心源码放在**工程根目录`LVGL/`**，无需额外`ThirdParty/`。
2. 曾通过`git -C .\LVGL describe --tags --always`得到**`v9.6.0-71-g5160f4d4a`**，说明最初克隆版本并非固定到v9.6.0发布标签。
3. 用户决定下载官方GitHub发布的 **v9.6.0 ZIP** 并用于STM32，而不是频繁使用Git切版本；2026-10-16实际读取`LVGL/include/lvgl/lv_version.h`，major/minor/patch为**9/6/0**且INFO为空，CMake版本文件也为9.6.0。该分发包根`lv_version.h`仅是转发头，不应再固定假设版本头位于`src/`。
4. 用户已把**`lv_conf.h`放到工程根目录**，保留在第三方库之外，便于未来更换核心库。
5. CMake已使用`LV_BUILD_CONF_PATH`、`add_subdirectory(LVGL)`及`target_link_libraries(... lvgl)`，且配置日志明确显示：`Using configuration: .../03_F407_HAL_FreeRTOS/lv_conf.h`。
6. ARM GCC 10.3.1 + CMake + Ninja实测编译完成（603/604，最终`03_F407_HAL_FreeRTOS.elf`链接成功）；日志 RAM **13,448B/128KB（10.26%）**，Flash **34,460B/1MB（3.29%）**。
7. 该体积只代表**当前未实际调用LVGL显示功能的可执行文件**；不代表完整GUI所需ROM/RAM。旧日志出现`Enabling the building of ThorVG internal`；本轮关闭相关CMake选项后，该提示消失。`Linking CXX`仍存在，因为第三方CMake收集了受宏保护的C++文件；不能单凭CXX字样判断ThorVG开启或失败，不修改LVGL核心来消除该字样。
8. 更早上传的`lv_conf.h`文件头为**v10.0.0**，属于历史风险。本轮实际配置文件头已为v9.6.0；修改前与当前库模板逐行比较，仅`#if 1`及`LV_MEM_SIZE (32 * 1024U)`两处不同，无需重建或整份替换配置。
9. 本轮按用户“方案合适可直接迁移”的授权，仅收口H4.1：在根`CMakeLists.txt`的`add_subdirectory(LVGL)`之前，把`CONFIG_LV_BUILD_DEMOS`、`CONFIG_LV_BUILD_EXAMPLES`、`CONFIG_LV_USE_THORVG`、`CONFIG_LV_USE_THORVG_INTERNAL`设为`OFF CACHE BOOL ... FORCE`；根`lv_conf.h`对应关闭`LV_BUILD_DEMOS`、`LV_BUILD_EXAMPLES`、`LV_USE_THORVG_INTERNAL`。保留RGB565、32KiB、LV_OS_NONE及软件绘图；ST7796、GUI任务、FreeRTOS配置和`.ioc`未改。
10. 使用现有GNU Arm GCC **10.3.1**、CMake **4.2.3**、Ninja，在新目录`build/H4_1_Verify/`配置并完整构建成功，编译日志未发现`warning:`/`error:`/`FAILED:`；RAM **13,448B/128KiB**、CCM **0B/64KiB**、Flash **34,460B/1MiB**。ELF未发现`lv_`或所检索C++运行时符号，说明尚未实际引入LVGL运行功能；抽查vg_lite_tvg和gltf各一个C++对象，text/data/bss均为0。
11. 构建证据：`build/H4_1_Verify/h4_1_configure.log`、`h4_1_build.log`、`CMakeCache.txt`及`03_F407_HAL_FreeRTOS.elf`。缓存四项均为OFF；预处理验证根配置已启用、版本9.6.0、RGB565、32KiB和LV_OS_NONE。配置中的`Could NOT find PkgConfig`为可选探测信息，未阻断构建。代理进程初始PATH未包含CMake，首次命令未启动；随后按现有缓存工具绝对路径及进程内环境变量完成构建，没有修改系统环境或预设。

### H4.1 验收清单（2026-10-16本轮证据）

- [x] 核心源码版本：`LVGL/include/lvgl/lv_version.h`为9/6/0，INFO为空。
- [x] 根配置与当前v9.6.0模板匹配；`#if 1`、RGB565、32KiB、`LV_OS_NONE`、软件绘图均满足要求。
- [x] SDL/PC驱动未启用；按本地`LVGL/env_support/cmake/main.cmake`真实选项关闭examples/demos及ThorVG；头文件配置同步。
- [x] 新目录全新配置与构建成功，RAM/Flash见上述证据。为遵守禁止批量删除文件的规则，未执行会清理现有CMake缓存目录的`--fresh`；使用`cmake --preset debug-armgcc -B build/H4_1_Verify`及`cmake --build build/H4_1_Verify`，没有删除文件。
- [x] 用户进一步明确授权直接整体迁移；已有版本/配置/构建证据满足H4.1配置验收，已继续H4/H7代码实现；LVGL实机验收仍待进行。

### PC应用层迁移方案历史评审（本轮已按5.1实施）

- 已定位源工程`D:\.workspace\LVGL_PC_GJOBS`：实际版本为10.0.0-dev，`src/main.c`创建480×320显示；`MeasurementBackend`与`InstrumentServices`接口确实存在。保留F407 v9.6.0，分阶段迁移`measurement/`、`instrument/`和字体的方案合理；不复制PC HAL、SDL入口、PC FreeRTOS端口或PC整套构建配置。
- 路线保留为H4.1 → H4.2 → H4.3 → H5 → H6 → H7。用户本轮授权按合适方案直接实施，但同时明确要求一次一个小步骤，因此本轮只修改H4.1配置；应用代码迁移与链接验证留待H7，不提前标记完成。
- 用户粘贴的旧评审提到“11个C文件语法通过、10处格式警告、模型1,920B”，本轮未重复该测试，这些数值不能充当本轮迁移验收证据。实际已读到`instrument_ui.c`多个局部`InstrumentModel`副本；GUI任务目前512 words（2KiB）、FreeRTOS堆8KiB，迁移时需减少复制并实测栈余量。
- PC存储使用`fopen()`；板载持久化尚未实现，后续通过服务接口替换，未接后端时必须如实提示保存不可用，不能伪报保存成功。LCD与W25Q128的SPI1访问需要串行化。
- 新增迁移检查点：PC应用实际使用标准库`snprintf("%.3f"/"%.6f", ...)`；目标工具链使用`nano.specs`且当前没有显式链接浮点格式化支持。H7需验证或改用整数定点格式化；仅开启`LV_USE_FLOAT`不能替代C标准库浮点格式化。当前不为尚未接入的页面扩大内存池或改链接参数。
- Tick方案已查本地LVGL回调类型与HAL声明，并核对官方说明：可在H4.2采用`lv_tick_set_cb(HAL_GetTick)`，利用现有TIM6的1ms HAL时基；本轮未接入。

### H4.2~H4.3原实施预案（代码已合并实施，实机未验收）

- H4.2：在GUI任务中`lv_init()`，注册LVGL毫秒Tick来源（与FreeRTOS时基协调），创建480×320显示设备；使用**480×20像素RGB565局部缓冲，约19,200字节**（可根据后续RAM余量调整）；注册SPI Flush回调。初期单缓冲、阻塞式SPI，**刷新完全发送完成后**才通知LVGL区域刷新完成。需校正F407小端内存和ST7796 SPI传输的RGB565大端字节顺序。
- H4.3：仅上屏一个背景、标签和按钮，测试持续刷新；不把整套PC四页面和DMA、触摸一次性叠加上来。
- H5：SPI DMA分块刷新，确保传输缓冲稳定、CS拉高时机与`flush_ready`时机正确。F407 **CCM RAM不能用作SPI DMA的直接源地址**；显示DMA缓冲放可DMA访问的普通SRAM。

## 6. 其余长期阶段的目标边界（不得遗漏）

- **H6 FT6336U**：接线与地址确认、I²C GPIO引脚分配、RST/INT、坐标旋转/校准，LVGL输入设备接入；PB10/PB11当前已有OLED，需判断能否共享总线。
- **H7 四页面**：PC端已完成的页面迁移至实机，保持整体风格；由GUI任务统一运行LVGL，串口/算法通过FreeRTOS队列或任务通知发数据，避免别的任务直接改UI。
- **A1 测量算法**：圆心/直径、内外径、矩形宽高与轮廓拟合；工件尺寸和标注映射、公差上/下限、PASS/FAIL、记录及声光提示；早期用PC/串口模拟数据脱离摄像头验证。
- **C1 通信**：UART DMA + 帧头/类型/长度/序号/校验等数据格式设计和健壮性处理；考虑帧长、轮廓点数量、丢包/断线、缓存队列；通过PC先仿真MaixCAM2。
- **V1 视觉前端**：MaixCAM2 + MaixPy/OpenCV采集图像、圆环与矩形轮廓识别、异常轮廓过滤、输出紧凑数据帧。
- **V2 标定精度**：像素与毫米映射、标尺与镜头畸变/透视误差、多个标准尺寸工件对照、误差与重复性数据；为测控专业论文提供实验支撑。
- **D1 论文/现场验收**：系统总体方案、软硬件架构、LVGL显示与通信、算法流程、标定与实验、误差分析；以老师和学校最终要求为准。

## 7. 近期变更记录（按事实追溯）

| 版本/阶段 | 重要更新 |
|---|---|
| V1.2～V1.4 | 35002 14针核查、外部3.3V供电，单核心板与筑基底板区别 |
| V1.5 | 发现板载W25Q128共用SPI1，LCD_CS从PA4调整为PE0；PA4给Flash |
| V1.6～V1.8 | CubeMX设置、GPIO标签和SPI1 5.25MHz；ST7796命令/参数函数准备 |
| V1.9～V2.2（聊天记录） | HAL编译通过；采用FreeRTOS GUI任务内部初始化，并使用`vTaskDelay` |
| V2.3～V2.4（聊天记录） | `ST7796`和`GUI_Task`拆成独立模块；初始化使用`void`，无延时函数指针 |
| V2.5（聊天记录） | 黑白红绿蓝最终正常，`MADCTL=0x28`和`INVON=0x21`适配当前LCD，矩形验证通过 |
| V2.6～V2.9（聊天记录） | H4移植：选择根目录独立`LVGL`，将`lv_conf.h`移到根目录；选v9.6.0发布版；已完成CMake ARM编译但v9/v10配置文件需核对 |
| **V3.0（本文件）** | 从早期MD与完整对话恢复全部路线，详细区分H3已实测与H4.1待配置验收，固定下一步骤，并明确版本不匹配风险 |
| **2026-10-16 / H4.1配置验证** | 核实源码和根配置均为v9.6.0；关闭examples/demos/ThorVG；新目录ARM构建通过（RAM 13,448B、Flash 34,460B）；修正文档驱动目录及SPI方向，记录PC分阶段迁移边界；待用户复核H4.1 |
| **2026-10-16 / 授权整体迁移** | 用户结束逐节课程；完成阻塞SPI的四页面软件迁移及2项主机测试，ARM RAM104,480B/Flash725,964B；实际显示未测；用户接手SPI1_TX DMA的CubeMX配置 |

## 8. 下次从这里开始（避免重复问、重复做）

**当前H5：等待用户完成SPI1_TX DMA CubeMX配置并生成代码。** 用户最新授权已改为直接迁移、结束逐节课程；不要重启H4.1或继续要求用户自己写像素接口，相关代码已经实际迁入。最新代码/资源/测试证据见第5节和`build/GUI_Migration`，当前仍是阻塞SPI运行路径，未下载实测LVGL。

**用户回复“DMA配置已生成”后**：先git status与核验.ioc、dma.c、spi.c、中断、启动向量、CMake及USER CODE保留情况；与pre_cubemx逐文件备份比较。随后实现DMA刷新，确保CS/缓冲生命周期/flush_ready和错误通知正确。触摸/Flash条件继续按用户反馈核实；完整四页面已存在，不再重复复制。任何阶段只在用户提供真实照片、日志或运行数据后升级为实机验收通过。主路线H6/H7验收后继续A1/C1/V1/V2/D1。

> **进度同步约定**：用户要求 Codex **每次阶段推进都直接更新工程根目录 `PROJECT_PROGRESS.md`**，并明确说明更新了哪些状态。回答中至少保留“完成、当前、下一步、后续主路线”。若文件未实际保存，不得声称已同步。

## 9. Codex交接与自动更新协议（必读）

本文件与同级 `AGENTS.md` 配套使用：`AGENTS.md` 提供每次会话的持续规则，本文件提供**完整进度和硬件事实**。

### 9.1 强制进度状态与用户确认

- 初始状态必须保持**H4.1进行中**，直到用户展示当前源码版本、`lv_conf.h`适配情况和构建证据。已知上次“能构建”不能替代配置验收。
- 对已做但未验证的操作标记为“待验收”，而不是“完成”。如实机通过用户亲自报告即可作为用户验证证据，引用“用户确认”而不是声称Codex亲测。
- 当用户说“完成了/下一步”时，先检查刚才步骤的验收条件。缺证据就追问最关键的一项，而不是默认完成。
- 每次更新时同时改动本文件**顶部当前步骤/下一步**、**第2节全阶段状态表**、**第5节当前任务（必要时改为新当前阶段）**、**第8节下次从这里开始**、**第10节追加进度日志**。历史已验证的任务、详细接线、引脚占用、长期阶段不可丢。
- 如果改变引脚、电源、软件版本或系统架构，先确认用户同意，更新“固定项目基线”和相关所有章节，避免互相矛盾。

### 9.2 H4.1 的本轮具体检查清单

- [x] 检查 `LVGL` 实际源码版本为9.6.0；证据位于`LVGL/include/lvgl/lv_version.h`。
- [x] 读取**当前工程根目录** `lv_conf.h`，与库内v9.6.0模板比较并核对关键宏。
- [x] 核对真实CMake开关，关闭examples/demos/ThorVG，并检查生成缓存；CXX字样不单独作为失败依据。
- [x] 新目录全新配置与编译，无编译警告/错误；RAM 13,448B、Flash 34,460B，当前未链接使用LVGL运行功能。
- [x] 用户授权直接继续整体迁移，H4.1配置验收已收口；当前转H5配置交接，详见顶部及第5节。

### 9.3 H4.2原定拆分（本轮代码已集中接入，实机结果待验证）

1. 在 `ST7796` 底层增加“按矩形区域写入原始RGB565像素”的函数（区别于纯色FillRect），验证边界和颜色字节顺序。
2. 在GUI任务初始化 `lv_init()`、创建480×320显示对象，准备约19,200B的480×20单局部缓冲（与当前SRAM占用核算）。
3. 实现并注册同步阻塞SPI的Flush回调：有效区域/步幅、字节序、传输结束后`lv_display_flush_ready()`。
4. 设置可靠的LVGL毫秒Tick，GUI任务周期调用`lv_timer_handler()`，不跨任务直接操作LVGL。
5. 实测显示及颜色正确后再标记H4.2通过；H4.3才开始标签/按钮的最小UI测试。

### 9.4 启动Codex时建议的第一条用户提示

> 请先按AGENTS.md读取PROJECT_PROGRESS.md，告诉我现在的具体编号、已验收证据、本轮要做的一步；先指导我，不要未经许可替我完成整个工程。等我做完并提供日志/照片，请直接更新PROJECT_PROGRESS.md中的状态、日期、下一步和变更日志。

## 10. Codex执行进度日志（持续追加，不覆盖既有记录）

| 日期 | 编号 | 本次事实/证据 | 状态变化 | 下一步 |
|---|---|---|---|---|
| 2026-10-16 | 文档交接 | 根据V3.0历史进度、当前对话和真实H4.1 ARM编译日志生成AGENTS.md + PROJECT_PROGRESS.md；尚未访问用户电脑工程源文件 | 固定H4.1进行中，不提前宣称配置验收完成 | 检查版本/根lv_conf.h/ThorVG/重新编译 |
| 2026-10-16 | H4.1 / 迁移方案评审 | 实际核实核心9.6.0、配置与模板匹配；根CMake关闭4项构建开关、lv_conf同步关闭examples/demos/internal ThorVG；用现有Arm GCC 10.3.1在build/H4_1_Verify全新构建成功，日志无编译警告/错误，RAM 13,448B、Flash 34,460B；实际驱动目录为User/Hardware；已定位PC源版本10.0.0-dev并认可分阶段应用层迁移；未迁移页面、未改驱动/GUI、未下载或实测LVGL | H4.1从“待核验”更新为“配置/构建证据齐全，进行中待用户复核确认”；其余阶段不升级 | 用户复核配置及日志确认H4.1；随后仅指导H4.2第1小步矩形像素接口；H7再核验API/字体、格式化、栈和总RAM |
| 2026-10-16 | H4/H7直接迁移、H5配置交接 | 用户明确结束逐节课程并授权整体迁移；实际新增User/GUI应用/字体/模拟后端与显示输入存储接口、RGB565驱动和SPI互斥、GUI队列及栈/堆调整；ARM编译无警告错误，RAM104,480B、Flash725,964B，2/2主机测试通过；未下载、未实测；用户确认SPI1已配置但DMA未配置并请求CubeMX指导 | H4.1已完成；H4.2/H4.3/H7配置/编译通过待实测；H5进行中等待用户生成DMA；H6硬件未开始；逐节课程按用户要求结束，未将未接硬件标为完成 | 用户添加SPI1_TX DMA2 Stream3 Channel3和优先级5的DMA/SPI中断并生成；下轮核验生成代码与RTOS向量后接DMA；触摸/Flash再确认 |
