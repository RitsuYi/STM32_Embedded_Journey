# PROJECT_PROGRESS — 基于STM32的智能视觉尺寸测量与质量检测系统设计

> **Codex长期实施计划与进度记录｜V3.1**  
> 更新日期：2026-10-17\
> **当前步骤：H6.5最终方向修复配置/编译通过待实测—用户通过光标确认上下镜像（点上面光标在下面，反之亦然），已将FT6336U_INVERT_Y设为1，保留SWAP_XY=1/INVERT_X=0。原始坐标经交换后只反转屏幕Y；临时OLED诊断和青色光标均已关闭。尚未获得修复后点击通过反馈，不标H6实机完成。**\
> **紧接下一步：重新下载当前Debug固件并复位，直接点击底部四个页面入口及一个页面内按钮，确认所点即所得、上下方向正确且松手正常。临时圆点与OLED诊断默认已关闭，不再索要旧诊断照片或要求重新配置CubeMX。**\
> **下一个动作：用户反馈修复后页面点击和上下方向是否正常；确认后收口H6.5/H6并从H7四页面完整交互验收接上。原始端点/长期余量未额外实测，仍保留证据边界；不恢复H5已取消的测试。**\
> **教学安排：2026-10-17最新用户明确“已连接好触摸屏，请帮我写触摸屏驱动，完成H6，写完说明写了什么并用大白话讲思路”。该授权覆盖本轮H6所需驱动、共享总线保护和LVGL输入接入，覆盖先前逐小步由用户编写的节奏；不扩展到Flash/相机/测量算法。**\
> **最近一次实施证据：FT6336U.h设置INVERT_Y=1（最终x=raw_y、y=319-raw_x）并关闭OLED诊断，lvgl_port.h关闭GUI_TOUCH_TRACE，同步中文注释；原四角映射、松手保留点及中间点的独立期望值已按物理方向更新。原build-debug编译链接退出0，build/Debug/h6_touch_y_invert_fix_build.log无编译warning/error/FAILED，RAM105328B/Flash753084B/CCM0。驱动回归1/1及关闭光标的LVGL输入测试1/1分别通过，ELF无临时光标/诊断函数符号。本轮未下载或取得修复后实机结果。**\
> **CubeMX生成策略：用户2026-10-17明确保留Delete previously generated files when not re-generated（DeletePrevious=true），由CubeMX清理不再生成的外设文件；撤回此前要求关闭该选项的验收条件。**\
> 记录规则：已完成与待验证严格区分；不把“编译通过”当成“实机功能验证通过”。

---

## 0. 使用说明（跨对话续接请先看这里）

这是项目**主进度文件**，Codex 在本工程新会话开始时必须根据根目录 `AGENTS.md` 主动读取本文件。每次有用户确认的进展、代码/构建证据或新阻碍，Codex 需要**直接修改此固定文件名**，更新日期、当前步骤、实际状态、待办和日志；**不得删除未完成的长期阶段**。与本文件不同的客观源码或测试证据需要核实后同步记录。不要无意义地另建 V3.2/V3.3 文件；以 Git 历史和文内日志追踪。

**当前最重要的事实**：H3五色/四角矩形已有用户实机验收。按2026-10-16用户授权，已保留F407 LVGL v9.6.0并迁入PC四页面、状态机、模型和五份字体，接入Tick/局部缓冲/阻塞SPI刷新、模拟后端及按键消息队列。**H5.1配置、H5.2回调及H5.3信号量的代码/构建验收已完成**：DMA2 Stream3 Channel3、DMA/SPI1中断5/0与RTOS向量正确；保留用户选择的CubeMX自动清理。H5.3由Codex按明确授权实现，实际build-debug日志为`build/Debug/h5_3_tx_done_build.log`，当时RAM104,664B、Flash728,996B、CCM0，无编译warning/error。

**2026-10-17最新H5.5证据**：用户已把ST7796_WriteRGB565的tx像素块发送改为SPI1_Bus_TransmitDMA(tx, count * 2, pdMS_TO_TICKS(100))；实际读取确认字节交换、stride、错误判断及外层CS/递归锁保留，lvgl_port仍在整个函数返回后flush_ready。用户明确授权补充该函数注释，Codex实际添加了参数/长度约束、窗口命令、字节序、DMA等待及收尾的中文注释并对齐缩进；去除注释/空白比较确认可执行C代码与用户保存版本一致。原`cmake --build --preset build-debug`实际编译ST7796.c并链接成功，退出0，日志未检出warning:/error:/FAILED:；RAM **104,672B/128KiB（79.86%）**、Flash **731,364B/1MiB（69.75%）**、CCM0，日志`build/Debug/h5_5_rgb565_dma_build.log`。Map确认DMA实际源tx.0位于**0x20006724、512B普通SRAM**；ELF保留DMA封装、HAL发送和两个回调/完成计数。

**H5.1～H5.5配置/代码/构建已验，H5.6正常链路按用户确认收口，H5.7发送粒度优化本小步已通过代码与构建验收。** 用户此前明确无需再测，取消的重复观察/异常测试不恢复；原单次截图保留。当前H5记为配置/编译通过待实测，新增优化的实际效果及42MHz长期稳定性未新增证据；记录这些边界后转H6.1准备，不把未测项写成通过或作为新阻碍。原接线及触摸/Flash边界保留。

**2026-10-17最新H5.7验收证据**：用户已保存tx=ST7796_WIDTH×2（960B）、tx_capacity=sizeof(tx)/2、count取remaining与容量较小值及对应中文注释，其他字节交换/stride/DMA等待/CS锁逻辑保留。原build-debug退出0，输出no work to do，未重新编译；源码UTC12:15:21、对象12:15:25、ELF/Map12:15:26，现有产物晚于源码。ELF及Map确认tx.0=0x20006724、0x3C0（960B）普通SRAM。按ELF节布局核算RAM105120B（80.20%，比H5.5增加448B）、Flash731364B（69.75%）、CCM0。日志build/Debug/h5_7_row_dma_build.log。本小步代码/构建验收通过，未下载或新增实机性能结果。

**2026-10-17早前电脑插件协助结果（历史）**：用户曾要求使用“电脑”插件边操作边讲解，指定入口初始化及重建会话均报`Windows Computer Use Sky runtime is unavailable`，当时未取得窗口、截图或实机数值，未进行UI操作或绕过插件。只读确认Cortex-Debug/J-Link及安全观察位置，转为请用户提供截图。这是电脑控制通道的历史阻碍；本轮已获得用户主动提供的变量图，不再要求重复打开侧栏，也未重新调用电脑插件。

**2026-10-17最新H6证据**：用户通过光标直接观察到上下相反并要求最终修复，已定位本模组屏幕Y方向配置不符，取代此前仅凭XY/N/P正常但具体原因未定的判断。已启用交换后Y镜像，关闭临时OLED诊断与光标；ARM编译通过RAM105328B/Flash753084B、驱动与LVGL输入两个目标各1/1通过。当前H6.5配置/编译通过待实测，等待修复后点击反馈，不能将根因定位或主机通过写成实机功能完成。

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
| 触摸 | FT6336U电容触摸，I²C2 PB10/PB11与OLED共线，PE1复位/PE2输入；用户确认已接线，驱动/LVGL已编译，待实测 |
| 存储 | 优先利用板载W25Q128 SPI Flash，使用前须协调与LCD共享的SPI1总线；也可后续按论文需求调整 |
| 供电 | LCD使用**外部稳压3.3V**，LCD电源GND与STM32数字GND**共地**；**不得直接并联两个3.3V电源正极** |
| 实施方法 | **2026-10-17用户最新要求**：已接好触摸，授权Codex编写驱动并完成H6软件接入，写完说明变更及通俗思路；当前授权覆盖先前H6逐小步教学节奏。保留既有迁入代码与用户改动，继续严格区分代码、编译、实机证据 |
| CubeMX生成策略 | **2026-10-17用户明确选择**：保留DeletePrevious=true，由用户运行CubeMX清理不再生成的外设文件，保持KeepUserCode=true；不要求关闭自动清理，不将其作为DMA验收门槛。该选择不授权Codex执行批量删除命令 |
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
│   │   # FT6336U.c/.h：触摸复位、读取、坐标及诊断
│   │   # I2C2_Bus.c/.h：OLED与触摸的统一互斥锁
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

| 编号 | 任务 | 当前状态（2026-10-17） | 通过标准 / 下一条件 |
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
| **H4.2** | LVGL Tick、局部缓冲、SPI Flush | **配置/编译通过待实测**（已有用户显示及一次诊断快照） | Tick/19,200B单缓冲、步幅/字节序及DMA等待flush已接入；快照flush105次/错误0，切页扫描感及完整显示验收仍待核验；主机契约测试为旧轮询版本历史证据 |
| **H4.3** | STM32上运行最小LVGL界面 | **配置/编译通过待实测**（用户取消独立课程，合并到H7） | 未另写临时标签/按钮程序；由迁入的完整四页面统一验收实机显示 |
| **H5** | SPI DMA分块刷新与性能优化 | **配置/编译通过待实测**（正常链路用户确认收口；H5.7粒度优化代码/构建已验） | tx=960B位于0x20006724普通SRAM，RAM105120B/Flash731364B；全宽行DMA 2→1为代码推算，性能/长期稳定性未新增实测，取消的测试不恢复；不标整阶段实机全部通过，转H6.1准备 |
| **H6** | FT6336U I²C电容触摸 | **配置/编译通过待实测**（H6.5上下镜像根因用户确认，最终修正待下载） | 用户点上光标在下，已设SWAP1/X镜像0/Y镜像1，关闭临时诊断；ARM RAM105328B/Flash753084B，驱动/真实LVGL输入两个目标各1/1通过。待用户下载验证四页点击/上下方向/松手后实机收口 |
| **H7** | FreeRTOS整合PC端四页面UI | **配置/编译通过待实测** | 应用/字体/模拟业务已迁入，主机测试通过；实机四页、完整触摸交互与资源余量待验证；持久化后端未接入 |
| **A1** | STM32几何拟合、尺寸标注和公差判定 | **未开始** | 圆环、矩形模拟轮廓计算尺寸；上下公差、PASS/FAIL、提示和记录 |
| **C1** | UART DMA通信协议、PC模拟视觉输入 | **未开始** | 串口帧同步、校验、超时、缓存/队列；模拟轮廓可驱动测量和UI |
| **V1** | MaixCAM2实际视觉算法 | **未开始** | 相机提取圆环/矩形轮廓，处理基本误检并发送STM32 |
| **V2** | 像素-毫米标定、精度和重复性测试 | **未开始** | 实际工件测量、误差统计、重复性/可靠性实验 |
| **D1** | 全系统测试、现场展示与毕业论文 | **未开始** | 演示自动测量→标注→公差判断→保存，形成合规论文和实验数据 |

### H5子步骤安排（2026-10-17明确，共7步）

此前路线只规定H5大阶段，H5.1～H5.4随教学推进细分，未明确总步数。2026-10-17按现有目标补全并固定为以下7步；问题排查留在对应步骤内，不因每轮对话继续新增编号。目前前5步通过仅指各自配置/源码与构建要求，不代表DMA链路已实测。

| 子步骤 | 内容 | 当前状态 | 验收条件 |
|---|---|---|---|
| H5.1 | CubeMX SPI1_TX DMA及中断配置 | 已完成（仅配置/构建） | DMA2 Stream3 Channel3、DMA/SPI1 IRQ 5/0、初始化顺序/RTOS向量/CMake正确并构建通过 |
| H5.2 | 完成/错误回调与诊断计数 | 已完成（仅代码/构建） | 唯一HAL回调、hspi1过滤、计数/错误码记录及编译正确 |
| H5.3 | 静态二值信号量与FromISR通知 | 已完成（仅代码/构建） | 独立于递归互斥锁，Init检查句柄，两个回调通知；原build-debug通过 |
| H5.4 | 一次DMA发送与任务等待封装 | 已完成（仅代码/构建） | 两个回调已先过滤hspi1再写tx_result；清旧通知、启动检查、结果区分、超时/错误安全停止代码核验通过；原构建退出0，修正源码已包含于现有产物，运行留后续验收 |
| H5.5 | LCD RGB565分块DMA及LVGL刷新接入 | 已完成（仅代码/构建） | 用户像素DMA调用已核实；命令轮询、普通SRAM tx、字节序/stride及整区域返回后flush_ready保留；已按授权补注释，实际原build-debug退出0，尚未实测 |
| H5.6 | DMA显示与任务唤醒实机验收 | 已完成（正常链路按用户确认收口；原额外实测未执行） | 既有显示/蜂鸣器反馈与单次DMA/错误/栈堆快照，用户明确结论无问题并要求不再测试；不宣称已补60秒/四页/两次增长/故障注入，未测项保留记录但不阻止推进 |
| H5.7 | 性能记录、稳定频率与H5收口 | 配置/编译通过待实测（发送粒度优化代码/构建小步已通过） | tx=960B、容量/上限联动、DMA等待与收尾正确；原build-debug退出0/no work，现有ELF与源码时间对应，tx普通SRAM960B、RAM增加448B。性能及稳定性未新增实测，不重启已取消的H5.6验证，不阻止H6.1准备 |

### H6子步骤安排（2026-10-17按用户代写驱动要求补齐）

| 子步骤 | 内容 | 当前状态 | 验收条件 |
|---|---|---|---|
| H6.1 | 模组资料、上拉及引脚/总线资源核查 | 已完成（资料/方案） | 用户原图及核心板资料已核，保留原证据 |
| H6.2 | 剩余引脚配置和接线 | 配置/编译通过待实测 | 原配置构建已通过，用户本轮明确“已连接好触摸屏”；未做通断或波形测量 |
| H6.3 | I²C2共享互斥及FT6336U复位/通信 | 配置/编译通过待实测 | 静态锁覆盖OLED/触摸全部HAL访问，复位时序/身份检查/模式/超时重试已实现；ARM及主机通过，真实应答待测 |
| H6.4 | 单指、横屏坐标及LVGL输入接入 | 配置/编译通过待实测 | 用户光标观察确认上下镜像，已设交换X/Y后屏幕Y=319-Y；X方向/端点保留，主机四角/中间点及松手回归通过，实际修正方向与交互待下载 |
| H6.5 | 实机触摸交互及共存验收 | 配置/编译通过待实测（最终Y镜像修复已编译） | 新固件关闭临时光标/OLED诊断，直接点底部四页及页面内按钮，验上下位置正确和松手正常；用户通过后收口H6并转H7 |

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
| 9 | LCD_MISO | **PA6 / SPI1_MISO AF5**已配置；用户本轮确认触摸接好，未单独提供MISO接线/读回证据；与板载Flash MISO共线需互斥片选 |
| 10 | FT6336U SCL | **PB10 / I²C2_SCL AF4开漏**，100kHz，与OLED共线；用户确认触摸已接，真实通信待测 |
| 11 | FT6336U RST | **PE1 / TP_RST**推挽输出、低速、无上下拉；GPIO初始低，GUI驱动拉低20ms后拉高并等待至少300ms；用户确认触摸已接 |
| 12 | FT6336U SDA | **PB11 / I²C2_SDA AF4开漏**，与OLED共线；用户确认触摸已接，真实通信待测 |
| 13 | NC | 不接 |
| 14 | FT6336U INT | **PE2 / TP_INT**普通输入、无内部上下拉、无EXTI；用户确认触摸已接，驱动记录电平但不以它限制轮询/松手读取 |

**当前SPI1配置（2026-10-17）**：Master，`.ioc`与`Core/Src/spi.c`为`SPI_DIRECTION_2LINES`，PA5/PA6/PA7 AF5，MISO未验证读回。8位；MSB First；Mode 0（CPOL Low、CPHA 1 Edge）；软件NSS；用户已将两处配置改为APB2 84MHz / 2 = **42MHz**，本轮H6未修改SPI配置或下载，实际SCK未仪器核验，尚不标为屏幕稳定频率。此前5.25MHz为历史基线。PA4为板载W25Q128 `FLASH_CS`；总线共享时仍需串行访问并协调模式，不能让两个任务同时直接操作。

**已有其他外设资源（以后分配不能覆盖）**：PB2=LED1，PB6=LED3，PC8=BUZZER，PE13/14/15=KEY3/2/1，PB10/PB11=I²C2（现有OLED），PA9/PA10=USART1，PA13/PA14=SWD。PB6已被LED3占用，故**不能直接把I²C1 PB6/PB7当作空闲引脚**；触摸H6需重新评估现有I²C2与触摸共总线可行性及地址冲突、上下拉/总线负载。板载TF、USB、Flash资源同样要复核，避免照搬其他F407开发板方案。

### 2026-10-17 SPI1共享与42MHz方案澄清（未改硬件/配置）

- **连接依据**：当前工程为PA5=SCK、PA7=MOSI、PA4=Flash_CS、PE0=LCD_CS；LCD_MISO暂不连接。立创针对天空星核心板的[SPI-FLASH教程](https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/beginner/spi.html)列出板载Flash的PA4/PA5/PA6/PA7连接，非筑基底板的PE4等分配。SCK/MOSI共线、各自独立CS属于可行的分时SPI共享；软件保证仅选中一个设备、DMA完成前不换设备，不因共线就必须重新分引脚。此为源码及官方资料核对，未重新测量实际接线。
- **尚未配置的读回线**：当前.ioc及spi.c只将PA5/PA7设为SPI1，未配置PA6/SPI1_MISO。板载Flash读ID/状态/数据前需补PA6的AF5及接收路径；这属于Flash接入准备，不是当前已通过的LCD写屏接线错误。板载Flash有PCB固定走线，不能只改CubeMX就迁移到另一组SPI引脚。
- **SPI1_Bus.c职责**：原先仅封装FreeRTOS静态递归互斥锁，H5.2已添加SPI1完成/错误回调诊断，H5.3已添加静态二值信号量及FromISR结果通知；H5.4用户已添加DMA发送/等待接口并修正回调过滤顺序，H5.5的ST7796_WriteRGB565现已调用此接口发送像素，代码/构建通过，尚未实测，未自动切换设备/时钟。独立文件不是HAL强制要求；若所有访问由同一任务串行安排可简化，但LCD和未来Flash跨任务共享时需要统一仲裁。现有ST7796调用它，SetAddressWindow内部又调用带锁的命令/数据函数，所以当前使用递归锁有源码依据，不直接删除该文件或改成普通非递归锁。
- **频率分别评估**：[STM32F407数据手册](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)第3.24节给出SPI1最高42Mbit/s；当前APB2=84MHz、/16为5.25MHz，/2可产生42MHz，但不代表从设备或外接线也保证该频率。以[W25Q128JV Rev G](https://www.winbond.com/resource-files/w25q128jv%20revg%2004082019%20plus.pdf)第9.6节为例，03h普通读最高50MHz，其他指令另有104/133MHz条件，且支持Mode0/3；42MHz本身不构成该型号的频率超限。板载Flash完整型号后缀仍待实物/资料确认，未据此标Flash实测通过。
- **LCD高频边界**：[ST7796S V1.0数据手册](https://www.displayfuture.com/Display/datasheet/controller/ST7796s.pdf)第7.4.3节/第54页，在VDDI=1.8V、VDDA=2.8V、25℃所列条件下，4-SPI写时钟周期最小66ns，对应约15.15MHz；42MHz不能从该表获得保证。具体35002模组/芯片版本与线路是否支持更高速需厂家规格或单独验证，不能把目标42MHz写成既定稳定值。当前5.25MHz保持不变。
- **以后不同设备可用不同速率**：取得同一总线锁，等待已有SPI/DMA彻底完成，两个CS均拉高，再在SPI空闲/按手册要求停用时切换分频/模式并选中新设备；事务结束后释放。切速功能尚未实现。本轮仅解释方案，未改频率、引脚或源码。

## 4. ST7796已通过的驱动实验（H3验收证据）

- 初期只有背光/雪花屏，后续通过独立测试验证了SPI显存写入通路。
- 颜色异常的排查记录：未设置反色时，红色曾显示为蓝色或黄色；在`MADCTL=0x20`并开启`INVON(0x21)`后呈现黑/白/蓝/绿/红（红蓝交换）；最终采用**MADCTL参数`0x28` + 命令`INVON 0x21`**，黑白红绿蓝顺序全部正确。
- 目前模组验证成功的最小初始化顺序：**硬复位 → 0x11 SLPOUT → 0x3A/0x55 RGB565 → 0x36/0x28横屏+BGR → 0x21 INVON → 0x29 DISPON → 开背光**。上述组合是**本模组实测适配值**，并非可以通用于所有ST7796面板的固定参数。
- `ST7796_SetAddressWindow()`设置`0x2A`列和`0x2B`行；`RAMWR 0x2C`连续发送RGB565像素；`ST7796_FillScreen()`以及`ST7796_FillRect()`采用小块缓冲与边界裁剪。
- 用户已报告**四角不同颜色矩形及中间白色矩形都显示正确**：H3.4通过。
- 后续LVGL Flush**不能简单复用纯色`FillRect()`**（它只生成统一颜色）；需提供“根据传入像素缓冲区原样发送一个矩形区域”的独立底层接口，并注意RGB565**字节顺序**。

### 尚需观察的硬件问题

用户曾报告STM32核心板附近有非常轻微的滋滋声，且未启动LCD时也有。尚未确定具体来源；可能是供电电感/电容或外接蜂鸣器，并**不能无证据认定为正常或损坏**。如果声音明显变大、供电异常、芯片发热或反复复位，应断电检查。当前项目实物显示功能已正常运行，此事为非阻断但需继续观察的风险。

## 5. 当前软件迁移结果与H5/H6交接

### 5.1 2026-10-16 用户更新范围及实际实施

2026-10-16用户明确要求跳过逐节步骤、直接按方案迁移并结束LVGL课程；同时要求需要CubeMX时停下来交给用户。该授权是以下已实施代码的历史依据。2026-10-17用户重新要求教学模式、一次一个小步骤，后续按这一最新要求指导，保留已有代码及全部阶段和验收边界。

- **驱动与显示**：新增`ST7796_WriteRGB565()`，接收原生小端RGB565及源步幅；检查边界，逐像素高字节先发，命令/数据发送返回值逐次检查，错误时拉高CS并解锁。新增SPI1递归互斥模块，既有驱动接口也按事务加锁。沿用MADCTL=0x28、INVON及5.25MHz，无引脚更改。
- **LVGL适配**：`User/GUI/lvgl_port.c/.h`创建480×320显示对象与480×20/19,200B单缓冲；取活动缓冲实际stride，同步传输返回后flush_ready，错误计数可调试观察。缓冲链接地址**0x200016F4**，位于DMA可访问的普通SRAM。本轮未实现/启用DMA。
- **应用层**：从`D:\.workspace\LVGL_PC_GJOBS\src`迁入measurement、instrument及fonts，保留相对目录和字体许可；源核心10.0.0-dev，目标仍为9.6.0。仅改目标副本，源PC工程未改；不迁SDL、PC主入口、PC FreeRTOS端口或fopen存储。
- **业务与交互**：`gui_demo.c`迁出模拟测量/标定逻辑，使用MeasurementBackend及InstrumentServices，结果保留simulated标志，不能作为MaixCAM2/实测算法证据。`gui_app.c`组装四页；KEY1上页、KEY2下页、KEY3当前主动作，原LED/蜂鸣器动作保留。按键经静态队列进入唯一GUI任务；数值编辑等完整交互仍待触摸。
- **输入和存储边界**：GUI_RegisterTouch是指针设备读回调接口，未接FT6336U硬件；GUI_StorageOps是load/save接口，未接W25Q128持久化。目前GUI_AppInit(NULL)，UI明确显示“模拟测量/仅内存”，设置、标定、记录重启后丢失，不伪报Flash保存成功；接入后端若保存失败会回滚模型。
- **内存与兼容修复**：LVGL池初试64KiB时普通SRAM链接占用120,864B（92.21%）；依据主机压力测试改为**48KiB**，保留更多普通SRAM。FreeRTOS堆改为24KiB，GUI栈2048 words=8KiB；多个1920B局部模型副本改成GUI静态回滚快照，初始化避免大复合字面量占栈。整数格式改用PRIu32，C snprintf浮点输出显式链接_printf_float；LVGL自身LV_USE_FLOAT仍为0。启用分配/空指针断言和GUI栈水位诊断。
- **可观察量**：`g_gui`记录初始化、断言文件/行、GUI剩余栈words、FreeRTOS及LVGL堆余量；`g_gui_display`记录刷新/SPI错误/耗时。这些运行值尚未从实机取得。ARM .su中用户应用最大单函数静态栈为instrument_calculate_calibration **1232B**，不代表整条调用链或实机栈已验收。

### 5.2 2026-10-16整体迁移验证证据（保留历史）

1. **ARM Debug编译链接通过**：目录`build/GUI_Migration/`，日志`configure.log`、`build.log`；GNU Arm GCC10.3.1+CMake4.2.3+Ninja，最后构建未发现编译warning/error。RAM **104,480B/128KiB（79.71%）**、Flash **725,964B/1MiB（69.23%）**、CCM0；普通SRAM链接余量26,592B，包含24KiB FreeRTOS堆和48KiB LVGL池的静态预留，不等于运行时空闲堆。
2. **主机测试2/2通过**：`tests/gui_migration/`，使用MinGW GCC13.1和同一LVGL9.6.0/48KiB配置；日志`build/GUI_HostTest/test.log`与`Testing/Temporary/LastTest.log`。覆盖四页创建、重复切页/重绘、模拟标定、圆环/矩形测量、仅内存记录、持久化失败回滚、新增提示字形。
3. **主机池压力数据**：max_used **38,656B**、最终free **5,912B**、碎片14%、累计1415次flush。主机是64位指针环境，此数据用于发现应用分配问题，不能充当F407运行内存实测。
4. **底层契约测试**：直接编译实际ST7796.c并以HAL桩验证窗口端点、RGB565字节序、带padding行步幅、超过256像素的分块、非法区域拒绝、逐次传输出错后CS/锁正确收尾。仅证明软件契约，未证明LCD收到数据。
5. **未验证项**：未下载/上电跑LVGL；屏幕颜色、持续刷新、任务真实栈余量/峰值、SPI速度稳定性待实测。DMA、FT6336U、Flash持久化尚未实现或启用，不因课程结束标记为实机通过。

### 5.3 H5验收结果与H6引脚配置交接

**当前H6.5最终方向修正已编译，待用户下载验收。** 用户明确观察上下光标镜像，已用INVERT_Y=1修正交换后的屏幕Y，关闭两个临时诊断宏；软件测试及编译通过，实际按钮响应尚未取得修复后证据。以下保留历轮排障依据，不再重复要求已解决的启动或旧光标数据。

- **2026-10-17本轮H5.6收口依据**：已有显示/蜂鸣器反馈、单次DMA完成3673/flush105/错误0与资源快照，加用户明确确认作为正常链路的收口依据。上一轮布置的60秒/全四页/第二次计数增长不再要求，可控异常也不追加；这些项目未执行而不是已通过，H5整体及42MHz长期稳定性不据此完成。
- **H5.7本轮代码核验通过**：ST7796.c:229为960B静态tx、:233按sizeof计算480像素容量、:285以容量限制count；用户未只扩大缓冲而遗漏256像素上限。差异仅为容量及相应中文注释，字节交换、stride、DMA等待、CS/递归锁及flush_ready时机保留。全宽行由2次DMA减为1次，全宽整屏640→320次仅为代码推算，不承诺实测速度翻倍。
- **H5.7本轮构建/链接核验通过**：原`cmake --build --preset build-debug`退出0，Ninja输出no work to do，本轮没有强制重编。源码/对象/ELF时间依次为UTC12:15:21/12:15:25/12:15:26；nm及Map均确认tx.0=0x20006724、长度0x3C0=960B（普通SRAM，CCM0）。ELF的.data664B+.bss102916B+heap/stack预留1540B=RAM105120B，普通SRAM链接余量25952B；Flash由.data装载末端核算731364B。RAM相对H5.5增加448B，Flash总量相同；本次检查日志未检出warning:/error:/FAILED:，不能据no work声称本轮重做了全程编译。日志build/Debug/h5_7_row_dma_build.log；未重跑旧主机测试/新增测试代码/下载实测，也未修改业务代码。
- **下一项H6.1准备范围**：仅核查触摸引脚及I²C资源。35002接口10=SCL、12=SDA、11=RST、14=触摸INT；板端当前PB10/PB11为I²C2 OLED，PB6为LED3等占用保留。后续核对模块资料、地址与可用GPIO后再给用户具体接线，当前未分配/接线或生成驱动。

- **2026-10-17本轮截图证据（单次用户实机快照）**：`g_spi1_tx_done_count=3673`、`g_spi1_error_count=0`、`g_spi1_last_error=0`；`g_gui_display`的flush_count=105、flush_errors=0、last_spi_status=0、last_flush_ms=3、max_flush_ms=8；`g_gui`的ready=1、queue_dropped=0、stack_free_words=770、freertos_heap_free=14152、lvgl_heap_free=22140、lvgl_heap_peak=24032、lvgl_heap_fragmentation=2、assert_file=0x0、assert_line=0。栈值是GUI历史最小剩余770 words=3080B，不是已使用量；堆值为字节。支持已发生DMA完成回调、记录到的错误为0且当前有余量；不证明长时间稳定、最大负载栈堆安全、LCD确认收到每位或42MHz符合模组时序。无第二张快照，尚不能验收计数持续增长。
- **2026-10-17本轮配置及刷新路径核查**：.ioc与spi.c均/2，APB2=84MHz，对应配置42MHz；lvgl_port为480×20 RGB565单缓冲（19200B）及PARTIAL模式。ST7796_WriteRGB565逐行按最多256像素/512B交换字节后DMA并等待，一条完整480像素行需2次，全宽整屏320行需640次DMA。当前没有绘图与发送重叠，Debug实际构建参数-O0，均为可能的开销来源，尚未量化各自贡献。保持原引脚/MADCTL=0x28/INVON。
- **扫描感解释及测量边界**：42Mbit/s下307200B整屏像素的纯传输下限约58.5ms，理论上限约17.1整屏/秒，还未计绘图、字节交换、DMA启动/通知与调度。3/8ms仅为单次局部flush耗时，起点在LVGL绘好该区域之后，不包含该区域绘图，更非整次切页。逐块铺开与LCD读显存/MCU写显存不同步造成的撕裂可能同时存在，无实际画面视频不能定性。官方参考：[LVGL单/双缓冲说明](https://lvgl.io/docs/open/9.5/main-modules/display/setup)、[LCD撕裂原理](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/lcd_screen_tearing.html)。双局部缓冲只有配合异步发送才可能重叠绘图/传输，也不自动消除LCD撕裂；已记录的14针接口无LCD TE信号，14脚INT属于触摸，不能当TE使用。本轮不改驱动/缓冲或编译优化，不另开阶段。
- **上一轮观察任务（本轮用户明确取消）**：原布置60秒/四页及GUI_Task.c:62第二张快照比较计数增长，并可用视频区分铺开/撕裂；尚未取得新增结果。用户现确认结论无问题并要求直接下一步，此任务停止，不作为续接门槛；保留原条件和未执行事实，不声称已满足这些条件。

- **已通过的DMA配置**：`.ioc`及`Core/Src/spi.c`一致，为SPI1_TX → DMA2 Stream3 Channel3、Memory-to-Peripheral、Normal、外设地址不递增/内存地址递增、Byte/Byte、Low、FIFO Disabled；存在`hdma_spi1_tx`及`__HAL_LINKDMA(spiHandle,hdmatx,hdma_spi1_tx)`。
- **已通过的初始化/中断/构建清单**：`Core/Src/dma.c`启用DMA2时钟，DMA2 Stream3中断优先级5/0；`DMA2_Stream3_IRQHandler()`调用`HAL_DMA_IRQHandler(&hdma_spi1_tx)`。`main.c`先`MX_DMA_Init()`后`MX_SPI1_Init()`；生成CMake已纳入dma.c。
- **生成代码保留核查**：对比`build/GUI_Migration/pre_cubemx/`，启动文件、FreeRTOSConfig.h、根CMake的SHA256均未改变；启动向量仍直接路由`vPortSVCHandler`/`xPortPendSVHandler`/`xPortSysTickHandler`。main.c差异只有新增dma.h及MX_DMA_Init，既有USER CODE保留；SPI模式/8位/5.25MHz与引脚未变，未新增CubeMX第二套RTOS。
- **SPI1全局中断补齐通过**：`.ioc`已出现`NVIC.SPI1_IRQn`的5/0配置；spi.c调用`HAL_NVIC_SetPriority(SPI1_IRQn, 5, 0)`及`HAL_NVIC_EnableIRQ(SPI1_IRQn)`；`SPI1_IRQHandler()`调用`HAL_SPI_IRQHandler(&hspi1)`，DMA中断仍正确。依据本地HAL源码`HAL_SPI_Transmit_DMA()`会开启SPI_IT_ERR，此全局中断用于SPI错误处理；正常TX DMA完成通知走DMA中断。
- **自动清理选项按用户选择保留**：用户明确不取消Delete previously generated files when not re-generated，以免取消外设后遗留生成文件；当前`DeletePrevious=true`、`KeepUserCode=true`符合其选择。撤回此前关闭自动清理的建议及验收条件，后续不再要求关闭。用户反馈其他外设未受影响；本轮检查源码差异也未见其他外设配置变动，但不据此宣称重做了所有外设实机测试。Codex仍不执行批量删除命令。
- **H5.1实际构建证据（二次生成历史）**：沿用GNU Arm GCC10.3.1、CMake4.2.3及Ninja，对`build/GUI_Migration/`执行增量构建，自动重新配置并编译spi.c/中断文件后链接成功，日志未发现warning:/error:/FAILED:。RAM **104,576B/128KiB（79.79%）**，Flash **728,324B/1MiB（69.46%）**，CCM0；普通SRAM链接余量26,496B，不等于运行时空闲堆。日志`build/GUI_Migration/h5_1_spi_irq_build.log`，未执行fresh/clean，未改系统环境变量或工程预设。前一次部分配置构建日志`h5_1_dma_build.log`及Flash727,652B作为历史保留。
- **未验证边界（更新至本轮截图）**：像素DMA完成计数、局部耗时和余量已取得单次快照，但板上固件与ELF完整对应、持续计数增长、60秒/全部四页、整次切页耗时和可控超时/错误未验。此前2/2主机测试仅为旧轮询版本历史证据，本轮未重跑；旧FreeRTOS/HAL桩尚未适配TickType_t及DMA接口，后续重跑须同步适配。
- **H5.2源码验收通过**：本轮实际读取`User/Hardware/SPI1_Bus.c`已包含spi.h，存在三个volatile uint32_t诊断变量及两个唯一HAL回调；两回调都过滤`hspi != &hspi1`，完成时递增计数，错误时记录HAL_SPI_GetError并计数。回调中无阻塞/互斥锁释放/LVGL调用。此前磁盘未见内容的情况已由保存后的源码解决，不再要求用户重复保存/定位。
- **H5.2编译证据及边界**：用户提供`cmake --build --preset build-debug`终端日志，显示`[46/47] Linking CXX executable 03_F407_HAL_FreeRTOS.elf`成功及RAM **104,584B**、Flash **728,388B**、CCM0；构建路径为`build/Debug/`。只提供末段，不评价完整日志有无所有告警。Codex读取相应Debug对象文件，nm显示两个回调均为强定义及三个变量；ELF/Map只保留错误回调与其两个变量，发送完成回调/计数因尚无DMA发送引用而被--gc-sections丢弃，此阶段属预期，不能把调试器是否能看到完成计数作为运行验收。未重跑构建/主机测试，未下载或实测DMA。
- **H5.3实际实现与代码验收**：按用户明确要求，在SPI1_Bus.c新增`static StaticSemaphore_t tx_done_storage`及`static SemaphoreHandle_t tx_done`；Init保留递归互斥锁创建，另创建静态二值信号量并同时检查两句柄。私有`notify_tx_done_from_isr()`先检查空句柄，使用xSemaphoreGiveFromISR及portYIELD_FROM_ISR；完成/错误回调保留hspi1过滤和原诊断，更新状态后调用helper。信号量创建后为空，与mutex独立，未从ISR释放互斥锁/调用LVGL/阻塞。仅修改现有.c，未新增公开API或修改头文件，原CMake已纳入该文件。
- **H5.3实际构建证据**：使用现有Arm GCC10.3.1、CMake4.2.3、Ninja运行原`cmake --build --preset build-debug`，编译SPI1_Bus.c并链接成功，退出0，日志未检出warning:/error:/FAILED:。RAM **104,664B/128KiB（79.85%）**、Flash **728,996B/1MiB（69.52%）**、CCM0；相对H5.2增量为RAM80B、Flash608B。日志`build/Debug/h5_3_tx_done_build.log`；仅进程内设置工具环境，未改预设或系统环境，未fresh/clean/删除文件。源码检查及构建满足H5.3本小步验收；无DMA发送和任务等待，唤醒效果尚未实测。
- **H5.4回调修正已验收（2026-10-17）**：接口已写入SPI1_Bus.c/.h，已有参数/上下文/Normal模式检查、递归锁、启动前SPI/DMA Ready及EN/BSY检查、清旧通知、结果等待、超时/错误安全中止和解锁。此前两个回调在过滤前赋tx_result的问题已由用户修正：实际源码均先`if (hspi != &hspi1) return;`，再保留计数/错误码并赋tx_result，最后notify_tx_done_from_isr。回调无阻塞、LVGL或互斥锁释放操作。代码条件通过，源码改动由用户完成。
- **H5.4首次用户构建证据（历史）**：用户提供原`cmake --build --preset build-debug`成功链接末段，[67/68] Linking CXX executable；RAM **104,664B/128KiB（79.85%）**、Flash **729,012B/1MiB（69.52%）**、CCM0。只收到末段，不宣称完整构建全程无告警；当时因回调顺序问题暂未验收，现由下条修正后核验收口。
- **H5.4修正后构建验收证据（2026-10-17）**：用户报告“ok了”，Codex实际读取正确回调后运行原`cmake --build --preset build-debug`，退出0、Ninja提示`no work to do`，本次检查没有重新编译/链接。日志`build/Debug/h5_4_callback_order_build.log`无warning:/error:/FAILED:；核实SPI1_Bus.c修改时间UTC08:21:12.966、对应Debug对象08:21:17.232及ELF08:21:18.207，现有产物晚于源码。以GNU Arm size/objdump读取ELF节布局，RAM占用104,664B（data664+bss102,464+heap/stack1,536），Flash末端.data装载结束0x080B1FB4，对应729,012B，CCM0。H5.4仅代码/构建验收通过，转H5.5；未强制重编、未改源码或实测。
- **H5.4验收与安全收尾约束**：代码及ARM构建通过仅为第一步证据；真实DMA/ISR/唤醒需后续运行数据验收。超时/出错必须确认SPI/DMA已停止后才允许复用源缓冲和结束事务，CS及递归互斥锁由持锁任务收尾，不能在ISR释放；成功通知也不能提前触发整区域flush_ready。本地HAL的完成/错误/中止行为已核查，H5.5接入必须沿用这些约束。
- **2026-10-17 H5.4本轮指导及本地HAL核查**：实际读取HAL_SPI_Transmit_DMA、SPI_DMATransmitCplt、SPI_DMAError、SPI_DMAAbortOnError、HAL_SPI_IRQHandler、HAL_SPI_Abort及HAL_DMA_Abort。Normal完成路径先关闭SPI DMA请求并等待TXE/BSY，再调用完成回调；部分错误路径会先清SPI DMA请求，HAL_SPI_Abort只在请求仍置位时中止对应DMA，因此还要检查DMA Stream EN，必要时单独HAL_DMA_Abort，并等SPI BSY清零。不能仅看HAL_SPI_STATE_READY就宣称硬件停止。
- **H5.4接口约定（代码/构建已通过）**：`HAL_StatusTypeDef SPI1_Bus_TransmitDMA(const uint8_t *data, uint16_t size, TickType_t timeout_ticks)`，仅调度器运行时的任务上下文、8位/Normal发送使用；size以字节计，data位于DMA可访问内存并保持到返回。内部递归取/还总线锁，但调用方仍须在选CS/发窗口命令前持有外层锁，到CS高后才释放；接口不操作CS。状态由volatile tx_result在完成/错误回调内通过hspi1过滤后先设置再通知；启动前清旧信号、确认SPI与DMA均Ready，HAL_BUSY直接返回不误停已有操作，其余失败/超时先记录错误并安全中止。中止失败或SPI持续忙时进入已存在的Error_Handler，不返回去复用缓冲；timeout_ticks仅约束结果等待，不包含总线锁等待和异常收尾。H5.5已由LCD/flush路径调用，运行效果仍未验证。
- **H5.5实际接入及注释验收（2026-10-17）**：用户已在`User/Hardware/ST7796.c`的`ST7796_WriteRGB565()`内将tx像素块发送改为`SPI1_Bus_TransmitDMA(tx, (uint16_t)(count * 2U), pdMS_TO_TICKS(100))`；实际读取确认每块至多256像素/512字节、逐行stride、高字节先发转换、错误判断及goto done、外层CS/递归锁均保留。命令/窗口参数继续轮询；每块等待结束后才复用tx，整区域完成或安全终止返回后才由lvgl_port调用flush_ready。按用户明确要求，Codex只为该函数添加零基础中文注释、对齐DMA段缩进，说明参数/长度、窗口终点、指令和DC、普通SRAM、字节交换及等待/收尾；比较修改前后去除注释/空白的源码完全一致，执行逻辑由用户完成。
- **H5.5实际构建及链接证据**：运行原`cmake --build --preset build-debug`，本次实际编译ST7796.c并链接成功，退出0，日志`build/Debug/h5_5_rgb565_dma_build.log`无warning:/error:/FAILED:。RAM **104,672B/128KiB（79.86%）**、Flash **731,364B/1MiB（69.75%）**、CCM0；相对H5.4增加RAM8B/Flash2,352B。Map的tx.0为**0x20006724、0x200字节**，位于DMA可访问的普通SRAM。ELF nm检出SPI1_Bus_TransmitDMA、HAL_SPI_Transmit_DMA、两个强定义回调及g_spi1_tx_done_count，像素DMA链路不再因未引用被链接裁掉；这仅证明代码链接存在，不是运行/性能证据。未改配置/引脚/频率、未下载、未重跑主机测试。
- **命令阻塞与像素DMA混用说明（用户本轮疑问）**：当前LVGL调用链为flush → ST7796_WriteRGB565 → 轮询窗口命令/参数 → 各块DMA加信号量等待 → CS高/解锁 → 返回后flush_ready，并不经过单字节WriteCommand/WriteData。两者负责初始化/部分旧绘图接口；仅改它们不能切换当前LVGL像素路径。短命令使用轮询可避免逐字节DMA启动/中断开销；可用等待封装改为DMA，但不是本步必要工作。轮询是当前任务执行查询，DMA等待是GUI任务阻塞休眠，两者都按完成后继续执行，且总线锁覆盖整事务，故按当前约定不会乱序。若直接把单字节函数改为HAL_SPI_Transmit_DMA并立即返回，可能提前改变DC/CS及失去局部参数存储生命周期，不能仅替换函数名。DMA不自动提高SPI线速，实际性能留H5.7实测。
- **H5.6原正常链路任务及当前反馈**：保持原独立3.3V/共地/片选方案、SPI1 /16=5.25MHz，由用户用当前Debug固件连续运行60秒、KEY1/KEY2切换四页并观察显示，禁止带电插拔。本轮用户反馈肉眼观察没问题、蜂鸣器没有阻塞，主观刷新速度和未用DMA时差不多；已记录部分实机观察，但未明确确认时长/全部四页或提供量化数据。当前任务转为辅助用户查看调试数据，不写测试代码、不改频率、不进行错误注入。
- **H5.6电脑插件尝试及阻碍（2026-10-17）**：用户明确调用computer-use插件并要求边操作边解释。已读取SKILL及guidance/confirmations，使用指定入口初始化；报`Windows Computer Use Sky runtime is unavailable`，重置node_repl后重试同样失败，元数据检查显示config/nativePipe/Sky模块路径均缺少。尚未获得任何窗口对象/截图、未点击/暂停/继续板卡、未读取实机变量，不捏造调试状态。未绕过插件另写Windows自动化或修改用户应用设置；当前先请用户Ctrl+Shift+D打开VS Code运行和调试并发当前界面截图，再按实际状态一次一步指导；运行时恢复后可在用户授权范围内通过插件操作。
- **刷新速度反馈的解释边界（最新）**：配置42MHz，仍为单缓冲/逐行DMA等待；当前tx=960B及容量上限联动已通过代码/构建核验。历史局部flush3/8ms不代表新固件整屏帧率，整次耗时没有新测量，不声称实际提速幅度；不重复布置用户已取消的观察任务。
- **H5.6原快照条件（历史，已按用户要求取消重复验证）**：GUI_Task.c:62为lv_timer_handler返回后的安全观察点；原计划比较两次DMA/flush计数增长、错误0、ready1及任务余量。实际取得一张快照，后由用户确认正常链路结论并要求直接推进，未取得第二张，不继续索要或推定增长。
- **未验证记录与推进决定（最新）**：可控异常收尾、最大负载余量、整次切页耗时和42MHz长期稳定性没有新增证据；按用户“无需再测、直接下一步”的要求保留未测记录并推进，不伪报全部通过。H5.7粒度优化已通过代码/构建本小步验收，H5整体保留配置/编译通过待实测，下一次从H6.1准备接上。
- **本轮新注释规则同步**：工作期间发现用户更新AGENTS.md，新增所有代码/示例须配零基础简体中文注释的要求；已重新完整读取，回复中的H5.4示例按此解释参数/返回值、任务等待、中断结果顺序和异常中止。未修改或覆盖用户的AGENTS.md改动。
- **更早的续接核查历史**：同日首次读取时git工作区干净、无DMA请求或dma.c，复核旧ARM RAM104,480B/Flash725,964B和2/2主机测试；这些“未配置DMA/未新构建”表述仅适用于生成前，现已由上述生成与新构建证据更新。
- 触摸RST/INT、I²C共享以及Flash的MISO/后端另行确认；当前.ioc未见PA6 SPI1_MISO，不能将SPI1发送配置当作Flash读取已验证。

#### H6.1：2026-10-17引脚与I²C资源核查（已完成，仅资料/方案验收）

- **实际源码证据**：`.ioc`中PB10=I²C2_SCL、PB11=I²C2_SDA，PB6=LED3；`Core/Src/i2c.c:41`为100000Hz、七位地址模式，PB10/PB11为AF4开漏（只主动拉低，由上拉电阻产生高电平）、GPIO_NOPULL。`.ioc`未显式保存ClockSpeed参数，100kHz事实以当前生成源码为依据。`User/Hardware/OLED.h:21`为`(0x3CU << 1)`，即七位地址0x3C、HAL参数0x78；本地HAL说明要求七位地址先左移一位。当前CMake已纳入i2c.c/HAL I²C，根LVGL/CMake配置未改。
- **模块信号定义已交叉核对**：35002 V1.2厂家数据手册公开副本第3页及用户本轮接口表截图均列出电容版10=T_CLK/SCL、11=T_CS/RST、12=T_SDI/SDA、13=空脚、14=T_IRQ/INT，与既有接线记录一致；不能按电阻版丝印把11当触摸片选。方案10→PB10、12→PB11、11→PE1、14→PE2，13悬空，尚未接线。PE1/PE2在当前.ioc、GPIO和应用源码中未发现配置/使用；用户本轮确认无外接线，板载占用结论见下一条。触摸RST独立于已有LCD_RST=PB1；INT是触摸输出、MCU输入，不能作为推挽输出或LCD TE。
- **2026-10-17 PE1/PE2板载占用核查通过**：用户重申使用天空星STM32F407VGT6最小系统核心板，并确认PE1/PE2没有外接线。实际查看立创官方核心板V1.0原理图的层次总图、双路排针、按键/LED、TF/Flash及调试接口图片：PE[0:15]仅引出排针，PE1→右侧P2第18脚、PE2→P2第13脚，未接板载LED/按键、USB、TF或SPI Flash；可用于本方案触摸RST/INT，不需拆板载元件。层次总图确认支持100脚芯片家族、兼容表明确含STM32F407Vx，此结论依据核心板原图，不使用筑基底板外设分配。属于官方原图核查加用户接线反馈，未做实物通断测量。用户附的是35002接口定义表，仅证明针脚功能，未包含上拉电路。
- **地址与速率结论**：FocalTech FT6336U手册I²C时钟范围10～400kHz，现有100kHz可作为初始速率；Espressif官方FT6336U组件列默认七位地址0x38，另有0x48模块。当前模块预期0x38，对应STM32 HAL参数0x70（由0x38左移一位，读写方向由HAL处理），与OLED 0x3C不冲突；用户模块真实地址/应答尚未测得，不把通用默认值写成实测事实。
- **电气条件及用户证据（已补齐原理图）**：用户确认“OLED已接，VCC接3.3V”，新提供35002 PCB原理图右侧J3电容触摸接口明确显示R7=10kΩ从TP_SDI/SDA上拉到VCC、R8=10kΩ从TP_SCK/SCL上拉到VCC；结合模块现有外部3.3V供电，触摸SDA/SCL各有10kΩ上拉到3.3V的设计依据。J3的RST=TP_CS→14针接口11，INT=TP_IRQ→14针接口14，映射与接口表一致。原图左上XPT2046是电阻触摸选配电路，不作为FT6336U驱动依据。资料/资源方案验收通过，不再索要同一原理图；物料实际焊接、OLED自身上拉及并联等效阻值/上升沿未测，不把原理图当成实测波形。保持100kHz，当前无需额外并装或拆除电阻；通信实测再判断总线表现。LCD原外部稳压3.3V/共地方案保留，禁止将两个3.3V输出正极直接并联，所有改线在两侧断电后进行。
- **共享的软件条件**：`OLED.c:80/108`直接调用hi2c2，`OLED_Task.c`持续OLED_Update后延时10ms；main.c中的xOledSemaphore仅创建了二值信号量，未用于总线互斥。FT6336U以后由GUI任务读取，不能与OLED任务无保护地同时访问hi2c2；地址不同不消除任务竞争。接入时须使用同一个FreeRTOS互斥锁保护两者每次完整I²C事务，避免在ISR里阻塞读取。OLED整帧1024数据字节在100kHz下仅数据与ACK理论耗时约92ms，尚未计地址/命令；这是代码推算而非实测。应允许触摸在OLED各次128B页传输之间获得总线，避免用覆盖整次OLED_Update的大锁增加触摸等待。本轮只记录约束，不实现Bus层或改OLED。
- **H6.1验收结论**：软件占用、核心板板载占用及用户外接线反馈、35002接口表和电容触摸上拉设计均已核查，H6.1记已完成（仅资料/引脚与资源方案验收）。用户随后要求一次性代配全部剩余引脚，已按下一小节H6.2实施；真实触摸地址、复位通信、实际接线及读回尚未验收，不将H6整体标为完成。
- **资料依据**：[35002厂家数据手册V1.2公开副本](https://www.scribd.com/document/870668659/3-5%E5%AF%B8SPI%E6%8E%A5%E5%8F%A3%E6%A8%A1%E5%9D%9735002%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C)、[FocalTech FT6336U数据手册（Waveshare托管）](https://files.waveshare.com/wiki/common/FT6336U_Datasheet.pdf)、[Espressif官方FT6336U组件](https://github.com/espressif/esp-iot-solution/blob/master/components/display/lcd_touch/esp_lcd_touch_ft6336u/README.md)。公开模组资料不能替代用户实物版本与上拉电路核对。
- **核心板原图依据**：[官方原理图页](https://wiki.lckfb.com/zh-hans/tkx/hardware/schematic.html)、[层次总图](https://wiki.lckfb.com/storage/images/zh-hans/tkx/hardware/schematic/schematic_20240815_151823.png)、[P1/P2引出图](https://wiki.lckfb.com/storage/images/zh-hans/tkx/hardware/schematic/schematic_20240815_203023.png)。已实际查看原始PNG及板载外设分图，未将网页正文或搜索摘要当作走线证据；PDF支持包下载未取得有效文件，最终使用官方网页的原图，不声明已审阅PDF。

#### H6.2：2026-10-17按授权一次配置LCD/触摸全部剩余引脚（配置/编译通过待实测）

- **授权与实施范围**：用户最新明确“太慢了，请一次性配置好LCD剩下的所有引脚”，据此不再停在逐针指导。局部修改`03_F407_HAL_FreeRTOS.ioc`、`Core/Inc/main.h`、`Core/Src/gpio.c`、`Core/Src/spi.c`，新增/修改代码配中文说明；保留原用户未提交的SPI /2、DMA及其他改动，未更换现有LCD接线、MADCTL=0x28、INVON或频率。此次直接同步配置与初始化代码，没有启动CubeMX重新生成；后续CubeMX以已保存.ioc为准。13脚NC没有GPIO配置，也不接线。
- **PA6读回脚**：.ioc加入PA6=SPI1_MISO，PA5/PA6/PA7的Mode同步为Full_Duplex_Master，与原SPI_DIRECTION_2LINES一致；spi.c初始化/反初始化均加入PA6 AF5。只补齐硬件引脚，未新增LCD读回或Flash驱动，不能声称读ID成功。PA6同板载Flash共线，FLASH_CS=PA4高、LCD_CS=PE0独立及SPI1互斥规则继续保留。
- **触摸四线**：SCL/SDA沿用PB10/PB11 I²C2 AF4开漏、无内部上拉、100kHz；在.ioc显式保存I2C2.ClockSpeed=100000。PE1标签TP_RST，推挽输出/低速/无上下拉，先写低再初始化输出，保持FT6336U复位直到后续驱动按时序拉高；PE2标签TP_INT，普通输入/无上下拉，不启用EXTI。main.h同步两个端口/引脚宏，I²C2代码无需改动。当前没有触摸驱动释放复位或读取坐标，GUI触摸接口仍未注册硬件后端。
- **实际验证**：现有GNU Arm GCC10.3.1、CMake4.2.3、Ninja执行原`cmake --build --preset build-debug`，实际增量编译gpio.c/spi.c等源码并链接退出0；日志`build/Debug/h6_2_all_lcd_pins_build.log`无warning:/error:/FAILED:。RAM **105120B/128KiB（80.20%）**、Flash **731428B/1MiB（69.75%）**、CCM0；比H5.7 Flash增加64B，RAM相同。只在命令进程中设置原工具路径，未改系统环境/预设，无fresh/clean/删除文件。结构核查.ioc无重复键、25个连续索引且唯一引脚，PA6/PE1/PE2及I²C2配置与代码对应；这不是已用CubeMX GUI重新打开验证，实物未下载/接线/通信测试。
- **下一步与边界**：用户按第3节整张表在断电后一次补齐9/10/11/12/14脚，沿用外部3.3V与共地，OLED原线保留，13不接；完成后H6.3接共享I²C2互斥、FT6336U复位和地址/寄存器读取，再接LVGL输入。复位当前保持低是预期状态，未释放时不应要求I²C应答；不将H6.2配置/编译证据写成触摸实机通过。用户上拉原图已保存为`C:/Users/RitsuYi/.codex/visualizations/2026/10/17/01a149d0-b716-71e1-9ddd-b8058daf7000/h6_1_sources/35002_schematic_user.png`。

#### H6.3～H6.4：2026-10-17授权编写触摸驱动及LVGL接入（配置/编译通过待实测）

- **最新授权和接线证据**：用户明确“已连接好触摸屏，请帮我写触摸屏驱动，完成H6”，并要求完成后讲变更及通俗思路。据此直接完成H6软件工作，不再要求用户逐段编写；触摸接线以用户反馈记录，不推定MISO读回、真实地址/ID或UI功能已经通过。沿用3.3V/共地和已配引脚，未修改.ioc/引脚/时钟/第三方LVGL核心或此前用户改动。
- **共享I²C2**：新增`User/Hardware/I2C2_Bus.c/.h`，静态普通互斥锁在main USER CODE、OLED启动探测前创建；调度器未启动时单线程访问，任务期有限等待，中断及暂停调度器时拒绝。OLED_IsConnected和每次OLED_I2C_Write均取同一锁，HAL错误码读完才解锁；触摸每次寄存器读写同样取锁，不锁整个OLED_Update或复位等待，保留OLED原功能。原xOledSemaphore不承担此锁的职责。
- **FT6336U硬件驱动**：新增`User/Hardware/FT6336U.c/.h`及面向初学者的中文注释。GUI任务把PE1拉低20ms后拉高，释放后至少等300ms（延时额外加一节拍确保最小值）；七位地址固定0x38，HAL传0x70；读A3 ID预期0x64、A6固件及A8厂商ID，写DEVICE_MODE=0和G_MODE=0。身份不符拒绝初始化；20ms HAL超时、约25ms锁等待，失败不死循环。连续3次通信/锁失败后降为每秒尝试重新配置，不反复复位或无锁重置共用I²C。PE2只记录电平，轮询不依赖INT，避免漏松手。
- **事件与坐标**：连续读02～06五字节，低四位点数/12位坐标与高位按下0、抬起1、持续接触2分开解析；单指供LVGL，双指主动松手，非法点数/事件/坐标及通信失败都松手。默认原始X0～319、Y0～479，交换X/Y输出480×320，配合MADCTL=0x28；头文件有端点、SWAP_XY及INVERT_X/Y参数，支持根据实机方向/端点调整，不声称已自动校准。`g_ft6336u`记录通信/ID、读帧/按下/错误次数、原始和映射坐标及按下状态。
- **LVGL接入**：GUI_Task初始化FT并通过已有GUI_RegisterTouch注册硬件回调，约33ms由唯一GUI任务随LVGL输入定时器读取，其他任务不访问LVGL。输入对象分配失败走现有断言；硬件未就绪不停止显示/按键，后续回调尝试恢复。lvgl_port松手或读失败沿用最后有效点，不跳回(0,0)。实际周期受绘图/传输/调度影响，33ms不是实测响应时间。
- **实际ARM证据**：现有Arm GCC10.3.1、CMake4.2.3、Ninja运行原`cmake --build --preset build-debug`，CMake已有CONFIGURE_DEPENDS自动纳入新增User源/头文件，无须手改构建列表；实际编译链接退出0，日志`build/Debug/h6_touch_build.log`未检出warning:/error:/FAILED:。RAM **105280B/128KiB（80.32%）**、Flash **752628B/1MiB（71.78%）**、CCM0；相对H6.2 RAM+160B/Flash+21200B（包括输入链路带入的LVGL代码）。ELF含FT_Init/Read、Bus接口和g_ft6336u；静态增量不等于运行时空闲堆，新增LVGL输入对象从原48KiB池分配，其真实余量仍待测。未fresh/clean/删除文件/下载。
- **实际软件测试**：新增`tests/touch_driver`，MinGW GCC13.1以-Wall/-Wextra/-Werror编译真实FT6336U.c和I2C2_Bus.c，HAL/RTOS替身测试**1/1通过**；日志`build/H6_TouchTest/test.log`及`Testing/Temporary/LastTest.log`。覆盖调度前/任务/中断/暂停调度器的锁行为、复位低/启动等待、0x70地址及0x64 ID检查、四角映射、坐标高位屏蔽、按下/持续/抬起、双指/非法数据、INT两电平均读取、超时和锁失败松手/解锁、连续错误退避/恢复及错ID拒绝。此为主机契约证据，非实物I²C/屏幕证据；旧H5主机套件未重跑。
- **H6.5验收与下一步**：当前只剩实机证据，不能标H6整体已完成。下载当前Debug，单指点击四页入口/主按钮/数值输入，触摸四角附近并拖动松手；确认位置/方向正确、松手不持续按住，OLED继续更新、按键/蜂鸣器响应。提供反馈；故障时给g_ft6336u关键字段（ready、chip_id、last_status/error、read_count、raw_x/y、x/y、pressed）。真实0x38/0x64应答、原始端点/排线方向、UI及任务余量未测，不推定已通过；用户确认后接H7四页整体验收。

#### H6.5：2026-10-17 HAL_Delay启动卡死排障（源码修复/编译通过，待实机复测）

- **用户实际反馈**：程序无法运行，调试停在HAL_Delay末尾`while ((HAL_GetTick() - tickstart) < wait)`；用户未提供调用堆栈、uwTick或CPU寄存器值。实际源码Core/User中唯一显式HAL_Delay位于OLED_Init，main在启动调度器前调用；FT6336U使用vTaskDelay，尚未运行到触摸初始化的路径与用户现象吻合。
- **源码根因**：上一轮新增`I2C2_Bus_Init()`放在OLED_IsConnected/OLED_Init之前。xSemaphoreCreateMutexStatic经xQueueGenericCreateStatic→xQueueGenericReset进入taskENTER_CRITICAL；本地FreeRTOS/SRC/port.c的uxCriticalNesting初值0xaaaaaaaa，调度前退出临界区不会归零，BASEPRI保持configMAX_SYSCALL_INTERRUPT_PRIORITY=5<<4=0x50。HAL时基为TIM6、优先级15（.ioc、hal_conf及InitTick一致），被屏蔽后不再调用HAL_IncTick，uwTick不增长，OLED的HAL_Delay(100)无法结束。TIM6初始化/IRQ/回调及ELF强HAL_InitTick链路存在，不是缺少HAL_IncTick。此因果链由本地代码确认，未冒充取得实机BASEPRI/uwTick数值。
- **最小修复**：main USER CODE将I2C2_Bus_Init移至OLED所有启动检测、HAL_Delay、显示命令及刷新结束之后，仍在其他RTOS对象/任务创建前；I2C2_Bus_Lock先拒绝中断，调度前返回true且不要求锁已创建、不创建/取得RTOS对象；任务运行后必须锁存在并取锁，调度暂停仍拒绝。同步.c/.h中文注释，解释为何先完成HAL启动再创建RTOS对象。未修改HAL_Delay、TIM6时基/优先级、FreeRTOS内核、.ioc、LVGL或触摸引脚。
- **回归证据及边界**：扩展tests/touch_driver测试“未Init且调度前可通信、不创建/取还锁；运行任务且未Init拒绝；中断拒绝”。旧Bus代码实际失败在调度前未Init的取锁断言，日志`build/H6_TouchTest/h6_delay_before_fix_test.log`；修复后扩展套件1/1通过，日志`h6_delay_after_fix_test.log`。原有芯片/时序/坐标/事件/错误/重试用例仍通过。主机替身不模拟真实BASEPRI/TIM6，不将此称为实机卡死复现或恢复证据。
- **修复构建**：原cmake --build --preset build-debug实际编译main/I2C2_Bus/相关头文件依赖并链接退出0，`build/Debug/h6_delay_startup_fix_build.log`无warning:/error:/FAILED:；RAM105280B/Flash752628B、CCM0。未下载或恢复调试会话，不重跑无关旧GUI测试、未删除文件。
- **本轮唯一待验动作**：用户停止旧调试，重新下载当前Debug并复位，先确认能越过OLED初始化并进入GUI。若仍停在HAL_Delay，给调用堆栈及uwTick/tickstart/wait/BASEPRI/PRIMASK；恢复后再验触摸位置、方向、松手和OLED共存，当前不追加整阶段测试。H6/H6.5进行中，H7及所有长期主阶段保留。

#### H6.5续：2026-10-17启动恢复、触摸漏点采证（诊断版编译通过，根因待实机数据）

- **新增用户证据**：“显示正常，按钮正常，触摸点不动，乱点才偶尔输入一个命令”；本轮追问同一按钮按住约2秒，用户确认“长按也很难响应”。据此收口上一轮启动卡死的恢复反馈，但不能将触摸可用或某一硬件故障写成已证实；不继续索要已解决启动的HAL_Delay寄存器。
- **只读核查与未决项**：LVGL输入默认33ms，GUI唯一任务读取，OLED每页传输取还统一锁，默认原始320×480→交换X/Y后480×320。没有真实g_ft6336u、坐标或总线数据，无法仅由症状区分映射/端点不符、通信失败/重试、双指拒绝或实际采样过慢；不以长按失败证明哪一种，也不盲改SWAP/INVERT/端点、I²C/采样频率或灵敏度。
- **诊断改动（临时，不是正式UI功能）**：FT6336U.h新增FT6336U_OLED_DIAGNOSTICS=1；g_ft6336u增加out_of_range_count、read_gap_ms/max_read_gap_ms和last_event，分别记录原始范围拒绝、真实Read入口间隔及事件。OLED_Task仅把变量画入既有显存上方48像素，原OLED_Update次数及底部按键提示保留，不额外查触摸、不跨任务调用LVGL；用OLED_ShowNum等逐位函数，不引入printf/局部大缓冲。.su新增show_touch_diagnostics静态栈16B，任务本身16B；不等于已取得运行栈水位。
- **屏幕读法**：R=原始x/y、XY=映射x/y、N=点数/P=已接受按下；S=HAL状态（0成功/1错误/2忙/3超时）；OK成功帧/ER通信失败；OUT原始坐标越界、ID预期64十六进制、RD预期1；DT实际Read间隔ms、EV事件0/1/2/3，N0时忽略EV。raw/XY松手或失败可能保留最后一点，需同时看N/P和计数；多变量不是原子一致快照，长按便于读数。OK/ER显示最低6位，OUT5位，DT4位。
- **构建与测试**：原build-debug实际编译FT6336U、GUI_Task、OLED_Task并链接退出0，build/Debug/h6_touch_diagnostics_build.log无warning:/error:/FAILED:，RAM105296B（80.33%）/Flash753708B（71.88%）/CCM0。驱动主机套件新增75ms入口时间差、独立越界计数/事件检查，1/1通过，build/H6_TouchTest/h6_touch_diagnostics_test.log；只验证软件计数，不模拟真实坐标或触摸性能。本轮未下载，未改原GUI布局、坐标/速率/引脚/互斥或删除文件。
- **本轮唯一待验动作**：下载当前诊断版，在大屏左上角附近内侧单指按住约2秒，拍OLED或抄全5行，取得R/XY与OUT/ER/S/DT后作对应局部修复。查清后关闭诊断宏恢复OLED原显示，再验稳定点击/方向/松手及共存；H6/H6.5继续进行中，H7及全部长期阶段保留。

#### H6.5续：2026-10-17 XY/N/P正常、LVGL事件链路排查（追踪版编译通过，具体根因未定）

- **用户新证据**：XY横屏坐标、N/P正常，但LVGL没反应；追问确认底部页面切换也无响应，不能归因于仅测量按钮未标定禁用。用户未提供完整按下松手帧、错误计数或LVGL事件数；接受其坐标/状态正常反馈，不据此推定所有瞬态均正常或点击已经命中按钮。
- **补齐测试缺口**：原gui_migration_test直接lv_obj_send_event(CLICKED)，绕过了输入命中。新增tests/gui_migration/gui_touch_test.c及touch_stubs/main.h，CMake增加独立gui_touch目标，编译真实lvgl_port、LVGL9.6及四页面，只有SPI/触摸硬件由替身提供；运行真实LVGL定时器处理按下和松手，不直接lv_indev_read或发送CLICKED。基线4页触摸切换成功；同一导航按钮内移动12像素（仍在按钮内）后松手也能切页，未复现所谓屏幕滚动吞点击，不据此修改滚动阈值/核心代码。此测试仅证明软件模拟路径，不证明实物所有输入稳定。
- **输入追踪改动**：lvgl_port增加g_gui_touch读取/按下上报/释放/失败计数及LVGL PRESSED/RELEASED/CLICKED真实事件计数，监听仅计数、不停止处理或伪造点击；显式绑定默认显示设备。GUI_TOUCH_TRACE=1时创建10像素青色圆点，由lv_indev_set_cursor在LVGL内移动，按下显示、松手隐藏，不可点击/滚动，不挡控件。OLED最后一行由DT/EV改为LP/LR/LC，各显示计数最低3位；DT/EV仍在g_ft6336u。OLED仅读变量，不跨任务调用LVGL。未改FT坐标/灵敏度/采样、I²C速率、引脚或库核心。
- **验证证据**：新增gui_touch单独1/1通过，build/GUI_HostTest/h6_gui_touch_trace_test.log；4次完整点击计数为4/4/4，加入按钮内移动点击后LC=5，验证光标/监听没有拦截控件。原build-debug实际编译lvgl_port/OLED_Task/GUI_Task并链接退出0，build/Debug/h6_lvgl_touch_trace_build.log无warning:/error:/FAILED:，RAM105328B（80.36%）/Flash754708B（71.97%）/CCM0。光标/监听从既有48KiB LVGL池分配，真实余量待实机。未下载或取得新实机计数，未重跑旧GUI/SPI/驱动套件，不把新增测试1/1说成全部3项通过。
- **本轮唯一实机动作**：下载当前追踪版，按下并松开底部第二个页面按钮一次，反馈圆点是否跟手且松手消失，以及LP/LR/LC是否各加1或最后一行照片。依据具体停止环节继续局部修复；当前未锁定根因或声称修复完成。问题解决后关闭两个临时诊断开关，H6实机收口后再接H7，长期主阶段全部保留。

#### H6.5最终修正：2026-10-17用户确认上下镜像（配置/编译通过待实测）

- **根因证据**：用户明确“点上面小蓝点在下面，点下面小蓝点在上面”，并要求最终修复。光标由LVGL实际输入移动，因此说明输入能到LVGL，但本模组触摸坐标与显示的竖直方向相反；此前“XY正常”仅能说明数值/按下存在，不能替代物理位置对应。此次依据直接光标观察固定屏幕Y镜像，不再继续猜总线或回调注册问题。
- **最小修正**：FT6336U.h将FT6336U_INVERT_Y从0改1，保留SWAP_XY=1、INVERT_X=0及原始X0～319/Y0～479端点。已有map_point在交换/缩放后执行Y镜像，最终x=raw_y、y=319-raw_x，改变的是交换后的屏幕Y，不是原始Y，也未改液晶MADCTL=0x28/INVON、引脚、SPI/I²C速度或采样周期。同步.h/.c紧邻中文说明。
- **收掉临时界面**：FT6336U_OLED_DIAGNOSTICS=0、GUI_TOUCH_TRACE=0，编译时不包含OLED临时显示函数和青色圆点，重新下载复位后OLED显示原启动内容及按键提示。计数及可选诊断代码保留，不批量删除文件，不移除共享总线或事件接口；ELF未检出touch_cursor/show_touch_diagnostics符号。
- **验证证据**：实际驱动测试独立四角期望表更新为原始(0,0)→屏幕(0,319)、(319,0)→(0,0)、(0,479)→(479,319)、(319,479)→(479,0)；验证松手保留(479,0)、中间点(100,200)→(200,219)，其余复位/锁/错误/重试用例保留。驱动主机1/1通过，build/H6_TouchTest/h6_touch_y_invert_fix_test.log；关闭圆点后的真实LVGL指针四页/按钮内移动及事件计数目标1/1通过，build/GUI_HostTest/h6_touch_y_invert_fix_test.log。仅此两个针对性目标通过，未把它们称为全部旧套件通过。
- **ARM构建**：原cmake --build --preset build-debug实际编译FT6336U/OLED_Task/GUI_Task/lvgl_port并链接退出0，build/Debug/h6_touch_y_invert_fix_build.log无warning:/error:/FAILED:，RAM105328B（80.36%）/Flash753084B（71.82%）/CCM0。保留全部用户既有暂存/未暂存改动，未下载或取得修复后实机反馈。
- **验收续接**：根因已由用户光标观察定位，修正代码/编译及两个主机目标通过；H6/H6.5标配置/编译通过待实测，不标实机完成。用户重新下载当前Debug复位，直接点击底部四页及一个页面内按钮，确认上下位置和松手正常；通过后更新H6实机验收并进入H7，不继续要求旧诊断采数。

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

- **H6 FT6336U**：用户已通过光标定位上下镜像，最终方向修正为交换X/Y后反转屏幕Y；临时诊断关闭，ARM及两个针对性主机目标通过。等待修复后四页点击、上下方向及松手正常反馈；实机收口后进入H7完整交互。
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
| **2026-10-17 / 教学模式续接H5.1** | 用户恢复一次一个小步骤、自己动手；只读核查当前仍无SPI1_TX DMA配置，复核既有构建/主机测试日志；发现DeletePrevious=true，指导用户生成前关闭；仅更新进度文档，未改源码或运行新构建 |
| **2026-10-17 / H5.1生成核验** | 用户报告DMA配置已生成；DMA2 Stream3 Channel3及DMA中断5/0、DMA链接/初始化顺序/CMake和RTOS向量核查通过；实际ARM增量构建无编译警告错误，RAM104,576B/Flash727,652B；SPI1中断尚未开启、DeletePrevious仍为true，留在H5.1指导用户补正；未接DMA刷新或实测 |
| **2026-10-17 / H5.1验收、转H5.2** | 用户再次生成，SPI1中断5/0及HAL处理函数完整，DMA与原有配置/RTOS向量保留；新构建RAM104,576B/Flash728,324B、无编译警告错误。用户明确保留CubeMX自动清理，撤回关闭DeletePrevious的要求，H5.1仅配置/构建验收已完成；H5.2仅布置SPI1回调诊断，未改业务代码或启用DMA |
| **2026-10-17 / H5.2反馈及SPI共享评审** | 用户报告回调已写，但当前磁盘Core/User未检出，待保存/定位核验，未标完成；核实LCD/板载Flash共享PA5/PA7但独立PE0/PA4片选，Flash读回所需PA6尚未配置；解释Bus文件是递归互斥封装，42MHz需分设备核对，不因Flash共享就重分引脚；记录官方核心板资料及STM32/Winbond/ST7796S时序依据，未改源码/引脚/时钟、未新构建或实测 |
| **2026-10-17 / H5.2验收、转H5.3** | 已读到保存后的回调和三个变量，hspi1过滤/错误码记录正确；用户build-debug链接日志RAM104,584B/Flash728,388B，Debug对象与ELF/Map符号核对一致。H5.2仅源码/编译验收通过，DMA未启用、未实测；当前布置静态二值信号量和FromISR通知，未代改源码、未重复构建 |
| **2026-10-17 / 授权完成H5.3、转H5.4** | 用户明确要求Codex实现所贴信号量步骤；局部修改SPI1_Bus.c，新增静态二值信号量/初始化检查与两个回调FromISR通知，保留原锁/过滤/诊断；实际原build-debug构建退出0且无编译警告错误，RAM104,664B/Flash728,996B，日志build/Debug/h5_3_tx_done_build.log；H5.3代码/编译验收完成，未启DMA或实测。当前H5.4发送/等待接口仅确定目标，未编写 |
| **2026-10-17 / H5明确7步、指导H5.4** | 用户询问下一步及H5总步数；将H5.1～H5.7共7步和验收条件写入第2节，保留前3步事实及全部远期主阶段；核查本地HAL的Normal完成/错误/中止路径，指导任务级发送与等待代码，特别检查DMA Stream EN及SPI BSY后才放缓冲。当前H5.4仍待用户编写，未改业务源码或新构建/实测 |
| **2026-10-17 / H5.4用户构建、回调顺序待修正** | 用户build-debug链接成功，RAM104,664B/Flash729,012B；实际读取.c/.h已有发送/等待与异常收尾接口，但两个回调在hspi1过滤前改tx_result，需局部移至过滤后、通知前。H5.4保持进行中，本轮只指导顺序修正并更新文档，未改源码/重复构建/启用LCD DMA/实测 |
| **2026-10-17 / H5.4验收、指导H5.5** | 用户反馈ok，实际回调已先过滤hspi1再写结果及通知；原build-debug退出0/no work，对象/ELF晚于修正源码，ELF占用仍RAM104,664B/Flash729,012B。H5.4代码/构建验收通过；只指导用户将ST7796_WriteRGB565像素块调用改为DMA等待接口，保持CS/字节序/flush_ready时机。未代改源码/下载/实测，H5.5进行中待用户操作 |
| **2026-10-17 / H5.5验收、授权补函数注释** | 实际用户像素DMA调用与字节序/stride/CS锁/错误及flush_ready顺序正确；按明确要求仅补ST7796_WriteRGB565中文注释及对齐缩进，去注释/空白比较代码一致。原build-debug实际编译链接退出0、无编译警告错误，RAM104,672B/Flash731,364B，tx=0x20006724普通SRAM。解释单字节函数不在当前LVGL像素调用链、轮询命令与DMA等待按序可混用；H5.5仅代码/编译通过，转H5.6待实机 |
| **2026-10-17 / H5.6正常链路测试指导、记录提速要求** | 用户要求下一步并明确验收后提高SPI速率；只读复核配置/时钟、像素DMA及诊断结构、按键/调试配置和已有构建日志，布置当前5.25MHz运行60秒/四页切换及GUI安全位置两次快照。H5.7先安排/8候选10.5MHz、之后逐档验证；未改源码/时钟、未新构建/下载或取得实机结果，H5.6保持进行中 |
| **2026-10-17 / H5.6用户部分实机反馈、电脑插件运行时不可用** | 用户表示显示肉眼正常、蜂鸣器未阻塞，刷新主观接近原轮询；不会调试并明确请求电脑插件操作讲解。按技能入口初始化、重建JS会话再试均报Windows Computer Use Sky runtime is unavailable，缺少config/nativePipe/模块路径，未取得窗口或数值/执行UI。只读确认Cortex-Debug/J-Link配置与观察点，准备按截图逐步指导；H5.6保持进行中，未改源码/时钟/构建/下载 |
| **2026-10-17 / H5.6取得42MHz单次快照、切页扫描诊断** | 用户图示DMA完成3673次/flush105次、SPI及flush错误0、局部flush3/8ms、GUI栈水位770 words及堆余量；核实.ioc与spi.c已/2=42MHz，单缓冲及512B逐行DMA等待、Debug -O0。解释整屏纯传输至少58.5ms，局部耗时不能当整页帧率，扫描感需区分铺开/撕裂；H5.6保持进行中，H5.7据用户实际提速配置记进行中但未验稳定。仅更新文档，下一步连续切页观察及第二张快照 |
| **2026-10-17 / H5.6用户确认收口、转H5.7小步优化** | 用户明确结论无问题、无需再测并要求下一步；按已有单次快照及确认收口正常链路，未执行的60秒/第二快照/异常验证保留未测记录。本轮只指导tx 512→960B及sizeof容量上限联动、同步中文注释，由用户编译；预期全宽行DMA 2→1次、静态RAM+448B，尚未改源码或构建，不声称性能提升已实测 |
| **2026-10-17 / H5.7发送粒度优化代码与构建验收通过** | 用户完成960B静态tx、sizeof容量与count上限联动；原build-debug退出0/no work，对象/ELF晚于源码，nm/Map确认tx=0x20006724/960B普通SRAM；RAM105120B（+448B）、Flash731364B、CCM0。无功能性问题，本轮未改业务代码/下载或新增性能测试。H5保留配置/编译通过待实测，下一次接H6.1触摸资源核查 |
| **2026-10-17 / H6.1引脚及I²C资源核查** | 核实I²C2 PB10/PB11=100kHz、开漏无内部上拉，OLED七位地址0x3C；用户确认OLED已接且VCC=3.3V。35002电容引脚定义与原记录一致，形成PB10/PB11共线、PE1 RST/PE2 INT候选；PE1/PE2仅软件侧未见占用。FT6336U预期地址0x38待实测；发现OLED任务无共用总线互斥。H6转进行中，H6.1等待实物/上拉核对；仅更新文档，未改配置/代码或构建/接线/实测 |
| **2026-10-17 / H6.1 PE1/PE2核心板占用核查通过** | 用户确认两脚无外接线、使用天空星F407VGT6最小系统核心板并附35002接口表；实际查看立创官方核心板V1.0层次总图、排针和板载外设分图，PE1仅引出P2第18脚、PE2仅引出P2第13脚，未接板载外设。保留两脚作RST/INT，无需拆元件；接口表已确认但不能证明SCL/SDA上拉。H6.1仍进行中，只剩模块上拉电路待核；仅更新文档和保存查阅原图，未改配置/源码或接线/实测 |
| **2026-10-17 / H6.1上拉核查完成、授权一次配置全部剩余LCD引脚** | 用户原理图明确R7 SDA/R8 SCL各10kΩ上拉VCC，按既有模块3.3V供电收口H6.1资料/资源验收；用户嫌逐步推进太慢，明确要求一次配置全部剩余LCD引脚。已同步.ioc与初始化：PA6 AF5 MISO、PB10/PB11 I²C2显式100kHz、PE1 TP_RST推挽初始低、PE2 TP_INT输入/无EXTI，13 NC不接。原build-debug实际编译链接退出0，无编译warning/error，RAM105120B/Flash731428B、CCM0。H6.2配置/编译通过待实测，未下载/接线或实现触摸驱动；下轮从整表接线及H6.3复位/共享总线通信接上 |

| **2026-10-17 / 授权实现H6触摸驱动与LVGL输入** | 用户确认触摸已接，授权直接写驱动并解释思路；新增FT6336U/I2C2_Bus，OLED逐事务共用锁、GUI复位及注册输入、松手保留位置，保留用户既有改动。原build-debug实际编译链接退出0，RAM105280B/Flash752628B/CCM0、无编译warning/error；真实驱动主机1/1通过。H6转配置/编译通过待实测，当前H6.5等待用户下载验收真实通信、方向端点、点击/松手及OLED共存，未标整体已完成 |

| **2026-10-17 / H6.5启动HAL_Delay卡死修复** | 用户反馈无法运行；本地源码定位新建总线锁早于OLED启动HAL延时，调度前临界区保持BASEPRI=0x50屏蔽TIM6。已将建锁移至OLED启动操作之后，并允许调度前未Init单线程通信；任务期必须有锁。原build-debug修复编译退出0、RAM105280B/Flash752628B；扩展回归旧代码失败、修复后1/1通过。H6/H6.5进行中，等待用户重新下载复位确认能启动，未标实机修复通过 |

| **2026-10-17 / H6.5启动恢复确认、触摸漏点诊断版** | 用户显示/按键正常，触摸乱点偶尔响应且同一按钮长按也难响应。加入OLED临时5行数据及FT独立越界计数/真实读取间隔/事件，未改坐标/速度/灵敏度；ARM编译退出0、RAM105296B/Flash753708B，扩展主机1/1通过。当前等待左上角单指按住时OLED数据，未认定触摸漏点原因或实机修复完成 |

| **2026-10-17 / H6.5 XY/N/P正常，转LVGL事件追踪** | 用户报告驱动位置/按下正常但底部页面也不响应；新增真实输入→LVGL命中→按钮切页测试能切四页及按钮内12像素移动释放。输入追踪版增加青色不可点击光标和LP/LR/LC计数，ARM编译退出0、RAM105328B/Flash754708B，新增gui_touch单独1/1通过。实机问题尚未复现或修复，等单次点击事件数据，H6继续进行中 |

| **2026-10-17 / H6.5上下镜像定位及最终修正** | 用户观察点上光标在下、点下在上，已据此设交换后屏幕Y镜像（INVERT_Y=1），X/端点/硬件配置不变。关闭OLED诊断及圆点；ARM编译退出0、RAM105328B/Flash753084B，驱动四角/中点/松手回归与关闭圆点的真实LVGL输入两个目标各1/1通过。修复后实机待用户重新下载，H6配置/编译通过待实测，未标实机完成 |

## 8. 下次从这里开始（避免重复问、重复做）

**下一次从H6.5最终Y方向修正后的实机反馈接上。** 用户已观察上下镜像、代码已设INVERT_Y=1并关闭临时光标/OLED诊断，不再要求重做旧定位。先核对新固件直接点击四页/页面内按钮、上下对应及松手结果，通过后收口H6接H7。

**当前构建证据**：原build-debug最终修正编译链接退出0，build/Debug/h6_touch_y_invert_fix_build.log无编译warning/error/FAILED，RAM105328B/Flash753084B/CCM0。驱动含修正四角/中点/松手回归1/1通过，build/H6_TouchTest/h6_touch_y_invert_fix_test.log；关闭光标的真实LVGL输入目标1/1通过，build/GUI_HostTest/h6_touch_y_invert_fix_test.log。ELF无touch_cursor/show_touch_diagnostics，本轮未下载实测。

**下一步操作及验收**：重新下载当前Debug并复位，直接点底部四个页面入口及一个页面内按钮，确认上点在上、下点在下且松手正常；无需再观察临时圆点或OLED诊断。用户报告通过后更新H6.5/H6为实机验收通过，从H7接上；当前仅方向修正配置/编译通过待实测，全部长期阶段保留。

**当前总线方案结论**：既有接线及SPI1_Bus保留，新增I2C2_Bus专管OLED/触摸仲裁；不因扫描感重分引脚、改MADCTL或迁移总线。PA6/MISO已配置，Flash接入时仍需核对实际读回与完整型号，保持独立CS及统一仲裁。配置42MHz不等于LCD时序/稳定性已验收，HAL错误0也不代表LCD逐位确认；未经新授权不改引脚/时钟或删除Bus文件。

> **进度同步约定**：用户要求 Codex **每次阶段推进都直接更新工程根目录 `PROJECT_PROGRESS.md`**，并明确说明更新了哪些状态。回答中至少保留“完成、当前、下一步、后续主路线”。若文件未实际保存，不得声称已同步。

## 9. Codex交接与自动更新协议（必读）

本文件与同级 `AGENTS.md` 配套使用：`AGENTS.md` 提供每次会话的持续规则，本文件提供**完整进度和硬件事实**。

### 9.1 强制进度状态与用户确认

- 首次H4.1交接时，须在版本、`lv_conf.h`适配和构建证据齐备前保持进行中；该历史验收现已收口。后续以顶部最新当前步骤为准，不回退为H4.1，也不以旧构建证据替代新增功能验收。
- 对已做但未验证的操作标记为“待验收”，而不是“完成”。如实机通过用户亲自报告即可作为用户验证证据，引用“用户确认”而不是声称Codex亲测。
- 当用户说“完成了/下一步”时，先检查刚才步骤的验收条件。缺证据就追问最关键的一项，而不是默认完成。
- 每次更新时同时改动本文件**顶部当前步骤/下一步**、**第2节全阶段状态表**、**第5节当前任务（必要时改为新当前阶段）**、**第8节下次从这里开始**、**第10节追加进度日志**。历史已验证的任务、详细接线、引脚占用、长期阶段不可丢。
- 如果改变引脚、电源、软件版本或系统架构，先确认用户同意，更新“固定项目基线”和相关所有章节，避免互相矛盾。
- 用户已明确选择保留CubeMX自动清理不再生成的外设文件，撤回早先强制关闭DeletePrevious的要求；此选项不属于DMA功能验收条件。历史日志中的“关闭后再验收”只记录当时建议，已由本条及顶部最新状态取代。

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
| 2026-10-17 | H5.1 / 教学模式续接 | 完整读取AGENTS与本文件，开始时git工作区干净；只读核实.ioc/spi.c/中断/CMake/GUI仍无SPI1_TX DMA，核心9.6.0、48KiB LVGL池；复核既有ARM RAM104,480B/Flash725,964B及2/2主机测试日志，未新构建/下载/实测；发现ProjectManager.DeletePrevious=true并指导生成前关闭；用户最新要求恢复教学模式，本轮仅更新进度文档 | H5仍进行中，明确当前子步骤H5.1；H4/H7保留待实测；教学模式取代此前整体迁移节奏，不升级任何技术验收状态 | 用户关闭自动删除旧生成文件，保留USER CODE，添加DMA2 Stream3 Channel3及优先级5的DMA/SPI中断并生成；提供生成完成反馈或相关代码/截图，下轮先核验生成文件与构建 |
| 2026-10-17 | H5.1 / DMA生成核验与构建 | 用户报告“DMA配置已生成”；核查DMA2 Stream3 Channel3、Normal/Byte、DMA中断5/0、HAL_LINKDMA、初始化顺序、dma.c纳入CMake均正确；启动文件/FreeRTOS配置/根CMake与pre_cubemx哈希一致，main.c原USER CODE保留；实际ARM增量构建退出0且无编译warning/error，RAM104,576B/Flash727,652B，日志build/GUI_Migration/h5_1_dma_build.log；发现SPI1全局中断未开启且DeletePrevious=true；本轮仅更新文档与构建产物，未改工程源码/下载/实测 | H5.1部分条件通过但仍进行中，未升级为完整配置验收；H4/H7继续待实测，DMA刷新未启用 | 用户补开SPI1 global interrupt并设5/0，关闭自动删除选项，保留USER CODE并重新生成；下轮核查SPI1_IRQHandler/HAL_SPI_IRQHandler、配置保存与构建，再收口H5.1 |
| 2026-10-17 | H5.1验收 / 转H5.2回调诊断 | 用户再次生成并明确保留CubeMX自动清理，反馈其他外设未受影响；核查SPI1_IRQn及DMA2_Stream3_IRQn均5/0、SPI1_IRQHandler调用HAL_SPI_IRQHandler、DMA链接/初始化顺序/CMake正确，启动文件/FreeRTOS配置/根CMake与生成前备份哈希一致；实际ARM增量构建退出0且无编译warning/error，RAM104,576B/Flash728,324B，日志build/GUI_Migration/h5_1_spi_irq_build.log；撤回关闭DeletePrevious的要求。仅更新文档与构建产物，未改业务代码/下载/实测 | H5.1已完成（仅配置与构建验收）；H5继续进行中，当前H5.2回调诊断待用户动手；H4/H7仍待实测，DMA刷新未启用 | 用户在SPI1_Bus.c加入hspi1过滤的完成/错误回调及诊断计数并编译，提供日志；核验后再指导DMA发送/完成等待及实机链路 |
| 2026-10-17 | H5.2完成反馈待核验 / SPI1共享与频率评审 | 用户报告回调已写并质疑Bus层、LCD/Flash共线及42MHz；实际SPI1_Bus.c仍为原互斥锁接口，Core/User未检出回调和诊断变量，无新代码/构建证据。核实工程LCD_CS=PE0、Flash_CS=PA4、PA5/PA7共享，PA6未配；查官方天空星核心板SPI教程确认板载Flash PA4~7；参考STM32 SPI1最高42Mbit/s、W25Q128JV普通读50MHz、ST7796S 4-SPI写周期66ns（指定条件），明确不由共享或42MHz直接推导重分引脚/Flash不能用；Bus独立文件可选但当前承担递归互斥职责，未删改 | H5.2保持进行中，记录用户完成反馈与磁盘证据差异；H5.1及H4/H7状态不变。仅更新进度文档，未新构建/改源码/改引脚或时钟/实测 | 用户先保存新增回调到当前工程并说明路径，再核验代码与构建；之后接DMA发送/等待。Flash阶段补PA6并确认型号与速率；当前保持5.25MHz和原接线 |
| 2026-10-17 | H5.2源码/编译验收 / 转H5.3信号量通知 | 用户提供原build-debug任务成功链接末段：RAM104,584B/Flash728,388B、CCM0；实际读取SPI1_Bus.c已有spi.h、三个诊断变量及两个唯一回调，hspi1过滤/错误码记录正确，无ISR阻塞或LVGL调用。只读检查Debug对象两回调强定义及三个变量，ELF/Map显示未引用的Tx完成回调/计数被gc裁掉（尚无DMA发送，属预期），错误回调保留。未新构建/主机测试/下载/实测，用户日志非完整日志不声明全程无告警 | H5.2已完成（仅回调源码/编译验收），此前磁盘未见问题已解决；H5进行中，当前H5.3待用户添加静态二值信号量/FromISR通知；H4/H7仍待实测 | 用户在SPI1_Bus.c创建静态二值信号量，Init检查mutex和tx_done，在两个回调诊断后FromISR通知并按需yield；运行原build-debug提供日志。之后再接DMA发送/等待；本步不改头文件/显示路径或启用DMA |
| 2026-10-17 | 授权实现H5.3 / 转H5.4发送与等待 | 用户明确要求直接完成所贴信号量步骤并进入下一步；实际局部修改SPI1_Bus.c，新增tx_done_storage/tx_done、Init同时检查mutex和tx_done、带空句柄检查的FromISR通知helper，完成/错误回调保留原过滤/诊断后均通知。运行原cmake --build --preset build-debug退出0，本次增量日志无warning:/error:/FAILED:，RAM104,664B/Flash728,996B、CCM0；日志build/Debug/h5_3_tx_done_build.log，diff检查通过。未改头文件/显示路径/外设配置，未删除文件/下载/实测 | H5.3已完成（仅代码与编译验收），H5继续进行中；当前H5.4发送/等待接口待实现，DMA尚未启动，H4/H7仍待实测 | 从SPI1_Bus.c/.h的任务级发送接口指导接上：清旧事件、启动检查、等待并区分结果，错误/超时先终止DMA再放缓冲/CS；先完成构建验收，再单独接像素刷新与实机链路 |
| 2026-10-17 | H5子步骤总数明确 / H5.4发送等待指导 | 用户要求下一步及H5总步数；按现有大阶段目标补全为H5.1～H5.7共7步（排障留在原编号内），前3步仍仅配置/代码/编译验收。只读核实当前SPI1_Bus.c/.h、配置/CMake及本地HAL完成/错误/中止行为，给出任务级发送/等待与tx_result回调指导，明确DMA请求已关仍需确认Stream EN及SPI BSY；中止失败不返回复用缓冲。仅更新文档，未编辑业务代码、新构建、调用DMA或实测 | H5.4仍进行中，代码待用户添加；其余H5子步骤及远期主阶段不提前升级，H5.3构建证据保持有效 | 用户按指导添加.h类型/声明、.c任务头/tx_result/回调赋值与发送等待函数，先运行原build-debug并提供日志；本步暂不接LCD/flush。后续H5.5接入、H5.6实机、H5.7性能与稳定频率验收 |
| 2026-10-17 | H5.4用户代码/构建核验，回调顺序待修正 | 用户提供原build-debug成功链接末段[67/68]，RAM104,664B/Flash729,012B、CCM0；实际读取AGENTS/完整进度、git status、SPI1_Bus.c/.h及ST7796/flush，确认新增接口具备清旧信号、启动检查、等待及异常停止确认，且未由LCD调用。两个HAL回调在hspi1过滤前赋tx_result，其他SPI回调可能改写SPI1结果；本轮仅指导移至过滤之后、通知之前并实际更新文档，未代改源码、重复构建/主机测试或下载实测 | H5.4保持进行中，已有编译证据但代码顺序待修正；H5总数仍7步，H5.5～H5.7未开始，既有验收和全部长期主阶段保留 | 用户只调整两个回调中的tx_result赋值顺序，保存后运行原build-debug并发日志；下一轮核对源码和新日志，通过后转H5.5像素DMA接入 |
| 2026-10-17 | H5.4代码/构建验收，转H5.5像素DMA指导 | 用户反馈“ok了”；实际读取保存后的回调已先过滤hspi1、再更新诊断/结果、最后通知。原cmake --build --preset build-debug退出0/no work，日志build/Debug/h5_4_callback_order_build.log；核实源码UTC08:21:12、对象08:21:17、ELF08:21:18，现有产物晚于修正源码，size/objdump确认RAM104,664B/Flash729,012B、CCM0。Map像素tx.0=0x20006720/512B，仍普通SRAM。只提供ST7796_WriteRGB565像素调用局部替换指导，未改工程源码/引脚/频率/下载/实测；旧主机测试未重跑 | H5.4已完成（仅代码/构建），H5.5进行中待用户操作；H5.6/H5.7未开始，H4/H7仍待实测，全部长期阶段保留 | 用户只替换tx像素块发送为SPI1_Bus_TransmitDMA并加中文说明，保留字节序/外层CS锁/错误收尾/flush_ready时机，运行原build-debug提供日志；下一轮核验代码、普通SRAM链接及构建后转H5.6实机 |
| 2026-10-17 | H5.5像素DMA代码/编译验收及函数注释，转H5.6 | 用户要求检查DMA修改、给函数补注释，并询问为何保留单字节阻塞函数；实际修改正确，按明确授权仅给ST7796_WriteRGB565补中文注释和对齐缩进，去除注释/空白比较前后执行代码一致。原build-debug实际编译ST7796.c并链接成功，退出0、无warning:/error:/FAILED:，RAM104,672B/Flash731,364B、CCM0，日志build/Debug/h5_5_rgb565_dma_build.log；Map tx.0=0x20006724/512B普通SRAM，ELF含发送封装/HAL发送/两个回调及完成计数。核实LVGL直接调用RGB565接口，不经过单字节函数，解释短命令轮询/像素DMA等待及事务锁的顺序保证；未改其他源码/引脚/时钟、未下载实测或重跑主机测试 | H5.5已完成（仅代码/编译），当前H5.6待用户实机；H5.7及所有远期阶段保留，H4/H7仍待实测，H5不标整体完成 | 下一轮从H5.6正常显示/唤醒实测开始：确认原供电/共地/片选，用户下载后提供屏幕反馈和完成/错误计数、错误码及g_gui_display；可控超时/错误和真实栈堆余量仍待后续验证 |
| 2026-10-17 | H5.6正常链路实机任务指导 / 验收后提速要求 | 用户明确要求进入下一步、验收后提高SPI速率；完整读AGENTS/本进度、git status后只读复核.ioc/spi.c仍/16=5.25MHz、APB2时钟及根构建/LVGL配置、DMA调用/诊断/按键和原H5.5构建日志。指导原Debug固件连续运行60秒、KEY1/2四页切换，在GUI_Task.c:62的lv_timer_handler返回后记录两次快照，中间取消断点并继续运行/切页；要求计数增长、错误为0及实际正常显示，记录g_gui_display耗时和g_gui余量。提速归H5.7，先候选10.5MHz再逐档实测；仅更新文档，未改源码/时钟、新构建、下载或获得实机数据 | H5.6保持进行中，正常链路待用户实测；可控错误/超时和收尾等仍待验收；H5.7未开始但用户提速目标已记录，全部既有阶段保留 | 用户提交屏幕表现及两次诊断快照/数值，先验正常DMA链路，再完成H5.6其余条件；之后按教学模式指导/16→/8提高至候选10.5MHz，不直接预标高频稳定 |
| 2026-10-17 | H5.6用户部分显示反馈 / 电脑插件调试协助未能初始化 | 用户报告肉眼显示没问题、蜂鸣器无阻塞，主观刷新与未用DMA相近；明确表示不会调试并调用电脑插件，要求边操作边讲解。完整读取插件SKILL及guidance/confirmations，按指定入口初始化node_repl报Windows Computer Use Sky runtime is unavailable，重置会话再试同样失败，缺少config/nativePipe/Sky模块路径。未获得窗口/截图/实时数值，未进行UI输入或用其他自动化绕过；只读核对Cortex-Debug/J-Link配置、现有变量及GUI安全观察点，未改源码/调试配置/频率、新构建或下载。解释5.25MHz线速及单缓冲逐块等待下速度相近不能证明DMA无效，未伪造耗时/帧率 | H5.6保持进行中，已记录用户显示/蜂鸣器部分实机证据；完成/错误计数、耗时/余量、四页/时长及异常收尾待验收，H5.7提速安排保留，未标整阶段通过 | 先请用户Ctrl+Shift+D打开VS Code运行和调试并发当前侧栏/工具栏截图，按实际会话状态一步一步指导；若插件运行时恢复则在授权范围内操作，仍从GUI_Task.c:62安全断点和完成计数开始，补齐诊断后再推进提速 |
| 2026-10-17 | H5.6单次42MHz诊断 / 扫描感排查 | 用户提供标注42MHz的变量图：DMA完成3673、SPI错误/末次错误0，flush105/错误0/状态0/局部耗时3ms及最大8ms；ready1/队列丢弃0、GUI栈水位770 words、FreeRTOS堆14152B、LVGL堆空闲22140B/峰值24032B/碎片2%、断言0。只读核实.ioc与spi.c已/2（84MHz对应42MHz配置）、480×20单缓冲、逐行最多256像素DMA并等待及Debug -O0；计算整屏纯传输下限58.5ms。查官方LVGL缓冲与LCD撕裂资料，实际画面未取得，不定性铺开/撕裂；未控制电脑/改源码时钟/构建/下载 | H5.6保持进行中，新增单次运行证据；H5.7按用户实际提速配置改为进行中，频率稳定性及整次耗时未验；保留全部主阶段及历史低频路线 | 用户先禁用断点运行60秒并隔1～2秒切四页，观察扫描是否仅切页及静止画面/蜂鸣器；GUI_Task.c:62第二张快照验证两计数超过3673/105且错误仍0，同次运行比较；之后再据结果决定整次刷新测量或局部优化指导 |
| 2026-10-17 | H5.6正常链路按用户确认收口 / 转H5.7发送粒度优化指导 | 用户明确说结论没问题不用再测、直接下一步；已有42MHz标注单次快照及显示/蜂鸣器反馈作为正常链路收口依据，未补做60秒/全四页/两次计数增长或异常注入。重新完整读取规则及进度、git状态，核实磁盘仍tx[512]/256像素上限、SPI /2与LVGL单缓冲等配置。仅指导用户在ST7796_WriteRGB565扩tx为960B、按sizeof(tx)/2计算容量并联动count和中文注释；全宽整屏调用数640→320及额外448B为推算，未代改源码/构建/下载 | H5.6已完成（正常链路按用户确认收口，额外测试未执行）；当前H5.7进行中待用户修改与编译，H5整体未完成；所有长期阶段保留 | 用户只做上述局部修改，运行原cmake --build --preset build-debug提供日志；核验源码容量/上限、tx普通SRAM地址和960B长度及RAM增量；不重复上一轮实测 |
| 2026-10-17 | H5.7用户完成后代码/构建核验，转H6.1准备 | 实际读取用户保存的ST7796.c两处修改及中文注释正确，字节序/stride/DMA等待/CS锁等保留。原cmake --build --preset build-debug退出0/no work，未强制重编；源码UTC12:15:21、对象12:15:25、ELF/Map12:15:26。nm/Map的tx.0=0x20006724、0x3C0=960B；ELF节布局RAM105120B（80.20%，比H5.5增加448B）、Flash731364B（69.75%）、CCM0；日志build/Debug/h5_7_row_dma_build.log，无本次检查warning/error/FAILED。未编辑业务代码/引脚时钟、下载或实测，不重跑旧主机测试 | H5.7发送粒度优化本小步代码/构建验收通过；H5及H5.7整体保持配置/编译通过待实测，性能/长期稳定性未新增证据，不恢复用户取消的测试。当前H6.1接入前准备，触摸硬件未开始 | 下一轮只核查FT6336U SCL/SDA、RST/INT、当前I²C2 OLED共享条件/地址及可用GPIO，形成无冲突接线方案后交用户操作；不直接接线或实现整阶段 |
| 2026-10-17 | H6.1引脚与I²C资源核查启动 | 完整读取规则/进度和git状态，只读核实.ioc、i2c.c、GPIO、OLED驱动/任务、CMake及根LVGL配置；I²C2 PB10/PB11=100kHz、AF4开漏/GPIO_NOPULL，OLED七位地址0x3C，用户确认已接且3.3V供电。交叉核对35002电容引脚与FT6336U手册/官方组件，触摸预期0x38待实测；候选11 RST→PE1、14 INT→PE2，仅软件未见占用。发现OLED无共用I²C2任务互斥、xOledSemaphore仅创建未使用。未取得模块上拉/实物占用证据；只编辑进度文件，未覆盖已有改动，未改源码/配置、构建/接线/下载或实测 | H6由未开始转进行中，当前H6.1软件资源已核、实物/上拉待确认，不标完成；H5及全部长期主阶段状态保留 | 用户保持现有接线，断电确认PE1/PE2未另接外设并提供35002转接板正反面清晰照片或原理图，确认上拉及I/O电平后收口H6.1；后续接线/配置时落实统一总线互斥，不恢复H5取消测试 |
| 2026-10-17 | H6.1 PE1/PE2最小系统板占用核查 | 用户确认PE1/PE2无外接线并重申天空星F407VGT6核心板，附35002接口定义截图；实际查看立创官方核心板V1.0层次总图及排针、按键LED、TF/Flash、调试分图，PE1只引出右侧P2第18脚，PE2只引出P2第13脚，未连接板载外设，保留RST/INT分配且无需拆元件。截图确认电容版11=RST/12=SDA/14=INT，未包含上拉电路；本轮只更新进度文档并保存查阅的官方原图，未改业务代码/配置、构建/下载/接线或实测 | H6.1的PE1/PE2软件及板载/外接占用核查通过，H6/H6.1保持进行中，仅模块上拉待核实；其他主阶段状态保留 | 只补35002手册带电阻连接的PCB原理图页，核对SCL/SDA上拉及I/O电平；不再重复询问PE1/PE2是否空闲，通过后再指导接线/CubeMX小步 |
| 2026-10-17 | H6.1上拉资料验收 / 授权一次完成H6.2全部LCD剩余引脚配置 | 用户原图显示R7 SDA/R8 SCL各10kΩ上拉VCC，结合模块3.3V供电完成H6.1资料/资源核查；用户随后明确要求一次性配置全部剩余LCD引脚。已局部同步.ioc、main.h、gpio.c、spi.c：PA6 AF5 MISO及对应DeInit，SPI引脚Full_Duplex_Master，I²C2 PB10/PB11显式100kHz，PE1 TP_RST推挽初始低/无上下拉，PE2 TP_INT普通输入无上下拉/无EXTI，13 NC不接；附中文说明。原cmake --build --preset build-debug实际编译链接退出0，日志build/Debug/h6_2_all_lcd_pins_build.log无编译warning/error/FAILED，RAM105120B/Flash731428B、CCM0。IOC键/25个唯一引脚索引检查及diff检查通过，未启动CubeMX重新生成、删除文件、下载或实测，既有用户改动保留 | H6.1已完成（资料/资源方案），H6.2配置/编译通过待实测；H6整体仍进行中，FT6336U后端未实现，PE1保持复位属预期。同步完整接线表与README，不恢复H5取消测试 | 用户断电按整表一次补9/10/11/12/14脚、13不接；接线反馈后从H6.3共享I²C2互斥、触摸复位释放和地址/寄存器读取续接，不再逐针配置或索要同一原图 |

| 2026-10-17 | 用户确认接线并授权实现H6；转H6.5实机验收 | 新增FT6336U.c/.h及I2C2_Bus.c/.h，main USER CODE调度前建静态锁，OLED探测/命令/页与触摸每次HAL共用互斥；GUI复位低20ms/释放等待至少300ms，0x38地址/A3=0x64检查、固件/厂商ID及普通/轮询模式，单指事件/横屏映射、错误松手和每秒重试，接入GUI_RegisterTouch并保留松手位置。原build-debug实际编译链接退出0，日志build/Debug/h6_touch_build.log无编译warning/error/FAILED；RAM105280B/Flash752628B、CCM0，ELF接口/诊断保留。新增主机契约测试真实驱动+HAL/RTOS替身，MinGW -Wall/-Wextra/-Werror、1/1通过，日志build/H6_TouchTest/test.log。同步README及进度，未改.ioc/时钟/第三方库、未删除文件/下载/实测，保留既有用户改动 | H6.2补用户接线反馈；H6.3/H6.4配置/编译通过待实测；整体H6配置/编译通过待实测，当前H6.5未开始待用户，全部长期阶段保留 | 用户下载当前Debug后反馈单指四角方向、四页按钮/数值输入、拖动松手及OLED/按键/蜂鸣器共存；异常给g_ft6336u关键值，依据实测调映射，验收后转H7；不重复H5已取消测试 |
| 2026-10-17 | H6.5启动卡HAL_Delay：定位并修复上一轮建锁顺序 | 用户反馈无法运行、调试停在HAL_Delay末尾while，未提供堆栈/寄存器。完整读取进度及源码，唯一显式调用为调度前OLED_Init的100ms等待；新I2C2_Bus_Init在其之前，静态mutex创建经队列临界区使本地port的初始0xaaaaaaaa嵌套数不归零，BASEPRI保持0x50，TIM6优先级15被屏蔽，uwTick无法增长。已将main USER CODE建锁后移到OLED全部启动结束后，Bus_Lock允许调度前未建锁通信且任务期必须有锁，同步.h/README中文说明；未改HAL_Delay/时基/内核/引脚。扩展未Init启动路径回归在旧代码实际失败，修复后1/1通过，日志build/H6_TouchTest/h6_delay_before_fix_test.log、h6_delay_after_fix_test.log；主机替身不模拟真实BASEPRI/TIM6。原build-debug实际编译链接退出0，无编译warning/error/FAILED，日志build/Debug/h6_delay_startup_fix_build.log，RAM105280B/Flash752628B/CCM0。未下载/实测/删除文件，保留用户改动 | H6由配置/编译通过待实测转进行中，H6.5已尝试实机且启动失败；源码修复及回归/构建通过，恢复效果待实机，保留全部主阶段 | 用户停止旧调试、重新下载并复位，先确认越过OLED_Init/HAL_Delay并进入GUI；仍卡时提供堆栈与uwTick/tickstart/wait/BASEPRI/PRIMASK，恢复后续触摸位置/方向/松手及OLED共存验收 |
| 2026-10-17 | H6.5启动恢复用户确认；触摸漏点采证 | 用户反馈显示和按钮正常、触摸需乱点才偶尔响应；补充同一按钮长按约2秒也难响应。启动修复按反馈收口，触摸根因尚未确定。只读复核输入33ms、GUI唯一访问及OLED分事务锁，未取得原始/映射坐标或通信数据；新增FT独立越界计数、实际Read间隔/最大间隔、事件及临时OLED诊断开关，OLED上方48像素显示5行，保留原按键提示及刷新次数，不加触摸读事务/printf/局部大缓冲，不改坐标、采样/I2C速率、灵敏度或互斥。原build-debug实际编译链接退出0，build/Debug/h6_touch_diagnostics_build.log无编译warning/error/FAILED，RAM105296B/Flash753708B/CCM0；.su新增诊断函数静态栈16B。扩展主机75ms间隔/独立越界计数检查及原套件1/1通过，build/H6_TouchTest/h6_touch_diagnostics_test.log；未下载、未取得数据、未删除文件或覆盖用户改动 | H6/H6.5保持进行中；启动恢复用户确认，稳定触摸未通过；临时诊断代码/编译通过不当成漏点已修复，全部长期阶段保留 | 用户下载诊断版，在大屏左上角附近内侧单指按住约2秒，拍OLED或抄全5行；按R/XY/OUT/ER/S/DT判别映射与通信/采样问题后局部修复，查清关闭诊断宏再验稳定点击 |
| 2026-10-17 | H6.5 XY/N/P正常但LVGL无响应：补真实输入测试与事件追踪 | 用户报告XY/N/P正常，进一步确认底部页面切换也无反应。检查现有lvgl_port及页面/命中源码，原测试直接发送CLICKED存在输入覆盖缺口；新增gui_touch目标编译真实LVGL9.6/适配/四页面，以真实定时器消费按下松手，四页及按钮内移动12像素释放可切页，尚未复现实机问题。新增g_gui_touch上报与LVGL真实按下/松手/点击计数、显式显示绑定、GUI_TOUCH_TRACE青色不可点击光标（LVGL自身定位、松手隐藏），OLED末行改LP/LR/LC，不伪造点击或修改FT方向/速度/灵敏度。原build-debug实际编译链接退出0，build/Debug/h6_lvgl_touch_trace_build.log无编译warning/error/FAILED、RAM105328B/Flash754708B/CCM0；新增gui_touch单独1/1通过，build/GUI_HostTest/h6_gui_touch_trace_test.log，4次点击计数4/4/4、移动后LC5。旧套件未重跑，未下载/取得新实机事件，保留用户已暂存改动，不删除文件 | H6/H6.5进行中，驱动位置/按下正常按用户反馈记录，稳定页面交互仍未通过；未锁定实际根因或宣称已修复，全部长期阶段保留 | 下载追踪版对底部第二个页面按钮按下松手一次，反馈青色光标跟手/松手消失和LP/LR/LC变化或末行照片；按具体输入/命中/松手/应用环节继续修复，查清关闭两个诊断开关后再验H6并转H7 |
| 2026-10-17 | H6.5根因用户光标确认：最终修正屏幕Y方向并关闭临时显示 | 用户明确点上光标在下/点下在上并要求最终修复，据此定位本模组Y方向映射不符；FT6336U_INVERT_Y=1，保持SWAP_XY=1/INVERT_X=0及原始端点，最终x=raw_y、y=319-raw_x，同步中文注释。FT6336U_OLED_DIAGNOSTICS=0及GUI_TOUCH_TRACE=0恢复原OLED并关闭圆点，ELF无touch_cursor/show_touch_diagnostics，计数与可选诊断代码保留。更新实际驱动独立四角期望、松手保留点和中间点，驱动1/1通过，build/H6_TouchTest/h6_touch_y_invert_fix_test.log；关闭圆点的真实LVGL输入目标1/1通过，build/GUI_HostTest/h6_touch_y_invert_fix_test.log。原build-debug实际编译链接退出0，build/Debug/h6_touch_y_invert_fix_build.log无编译warning/error/FAILED，RAM105328B/Flash753084B/CCM0。未改LCD参数/引脚/总线速度/周期或用户已有改动，未删除文件/下载/实测 | H6/H6.5由排障进行中转配置/编译通过待实测；上下镜像根因已有用户实物观察，修正后点击未取得证据，保留全部长期阶段 | 用户重新下载当前Debug复位，直接点击底部四页和一个页面内按钮，确认上下对应及松手正常；确认后收口H6实机验收，从H7继续，不再要求旧诊断照片 |
