# PROJECT_PROGRESS — 基于STM32的智能视觉尺寸测量与质量检测系统设计

> **Codex长期实施计划与进度记录｜V3.1**  
> 更新日期：2026-10-18\
> **当前步骤：A1矩形测量闭环接入（指导中，待用户修改gui_demo.c）。A1.1矩形基础函数已通过源码/ARM编译核验及电脑数值检查；直接推进模拟像素角点→标定到毫米→长宽计算→页面/记录，不再按声明、编译、单次数值检查拆成多轮。**\
> **紧接下一步：仅局部替换gui_demo.c的step()结果生成段。矩形采用一组旋转像素角点，两种成像配置按1/2倍像素处理，先instrument_map_point再instrument_rectangle_dimensions；原模拟圆环路径与完成回调/记录链路保留，矩形不重复套旧尺寸标定。**\
> **下一个动作：用户保存替换段，原build-debug编译并下载，参考件50.00mm完成模拟标定、矩形/mm模式测量，统一反馈页面长宽32.00/20.00mm、记录同值及再次测量正常；另一成像配置重新标定后仍同值。源码接入/本次下载与实机结果尚未发生。**\
> **教学安排：用户最新明确“拆分太细了 请直接一点 一步就要做很多！”，覆盖此前一次一个很小步骤的节奏；改为每轮一个完整功能闭环，相关代码、编译和验证一起指导。仍由用户亲自修改业务源码，本轮未代改。Flash/真实轮廓/真实拟合残差/公差仍未完成，不因模拟验证通过升级。**\
> **最近一次证据：用户build-debug链接成功，RAM105184B/Flash754364B/CCM0；源码11:01:55UTC、对象11:01:58、ELF11:02:00，实际对象含新函数，最终ELF无该符号且map丢弃796B未引用段。电脑直接编译用户真实model/measurement源，4组矩形、6组无效输入及两种成像配置的旋转像素换算通过，日志build/A1_RectangleVerify/build.log、test.log；只证明软件数值，未下载/亲测。**\
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

**2026-10-18最新H6证据**：原H6/H6.5已按用户明确“验收完成”收口，调试显示/统计已清理。其他工作补充数字键盘底行五键下方命中，ARM/真实GUI输入回归通过，RAM105184B/Flash754364B/CCM0；新增范围保留单项待实测状态。原验收保持，不恢复中央/卡死/同组采数；本对话H7现有主流程已另获用户确认，主线转A1.1，不以同步新增范围重开H7。

**2026-10-18商家回复补充（上一轮）**：两张聊天图中商家建议批量定制触摸内部固件、少量项目由主控过滤偏离坐标，未提供具体算法、阈值、可用固件或本机检测结果。“未适配UI”属笼统说明，不足以证明换UI必须重写触摸固件；偶发坏点可考虑排除，持续偏报不能靠丢点恢复真实位置。上一轮仅解释并更新文档；本轮按用户新请求扩展UI命中区，不据商家建议恢复历史滤波。

**2026-10-18最新H7验收与A1交接**：H7现有四页功能与合并的H4.2/H4.3功能已按用户确认收口。A1.1用户保存的矩形函数已通过源码/ARM编译核验及电脑4组矩形、6组非法输入、两种成像标定的数值检查；当前按用户“拆分太细了”要求统一指导矩形像素角点→毫米→长宽→UI/记录闭环，接入代码及其实机待用户，见5.7。GUI_AppInit(NULL)、Flash/真实测量未接及资源峰值无新数值的边界保留；同步键盘补充按独立证据记录。

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
| 触摸 | FT6336U电容触摸，I²C2 PB10/PB11与OLED共线，PE1复位/PE2输入；2026-10-18用户明确确认H6.5实机交互验收完成，调试清理版代码/编译/回归通过，尚未由Codex下载 |
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
│   │   # FT6336U.c/.h：触摸复位、读取、坐标及错误重试
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

| 编号 | 任务 | 当前状态（2026-10-18） | 通过标准 / 下一条件 |
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
| **H4.2** | LVGL Tick、局部缓冲、SPI Flush | **实机验收通过**（显示功能随H7用户确认收口） | 四页正常显示/刷新按用户反馈验收；19,200B单缓冲及DMA等待保留，不新增量化帧率、压力或故障测试结论 |
| **H4.3** | STM32上运行最小LVGL界面 | **实机验收通过**（合并至H7，用户确认功能正常） | 未另写临时标签/按钮程序；由完整四页面统一完成界面功能验收 |
| **H5** | SPI DMA分块刷新与性能优化 | **配置/编译通过待实测**（正常链路用户确认收口；H5.7粒度优化代码/构建已验） | tx=960B位于0x20006724普通SRAM，RAM105120B/Flash731364B；全宽行DMA 2→1为代码推算，性能/长期稳定性未新增实测，取消的测试不恢复；不标整阶段实机全部通过，转H6.1准备 |
| **H6** | FT6336U I²C电容触摸 | **实机验收通过**（原H6.5用户确认；本轮键盘补充待实测） | 保留无调试显示/统计、原命中扩展及互斥恢复/方向/CONTACT；新增数字键盘底行五键下方到Y319的代码/ARM/GUI回归通过，实物效果待确认，不重开原整体诊断 |
| **H7** | FreeRTOS整合PC端四页面UI | **实机验收通过**（2026-10-18用户确认现有功能） | 四页显示/交互/模拟流程按用户确认收口；仍为仅内存/模拟后端，持久化未实现、资源峰值无新增量化证据；同步新增键盘范围保留单项待验记录 |
| **A1** | STM32几何拟合、尺寸标注和公差判定 | **进行中**（基础函数代码/编译与电脑数值检查通过；矩形闭环接入待用户操作） | 当前统一推进模拟像素四角→标定→长宽→页面/记录，见5.7；尚未替换gui_demo或取得该路径ARM/实机结果。完整轮廓拟合、圆环、公差和真实精度仍待后续，资源基线见5.6 |
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
| H6.2 | 剩余引脚配置和接线 | 实机验收通过（按H6.5用户确认收口） | 原配置/构建及用户接线证据保留；H6.5整体交互已确认，通断/波形仍无仪器测量记录 |
| H6.3 | I²C2共享互斥及FT6336U复位/通信 | 实机验收通过（功能按用户确认） | 已有ID64/坐标通信截图及“不再卡死”反馈；保留锁与限次自动恢复，清理调试快照和固件/厂商号查询，仅核芯片ID；不补造复位/总线波形 |
| H6.4 | 单指、横屏坐标及LVGL输入接入 | 实机验收通过（按H6.5用户确认） | 单指CONTACT、x=raw_y/y=319-raw_x、15ms目标及按钮命中扩展保留；GUI/驱动回归通过，调试数据与圆点已从源码删除 |
| H6.5 | 实机触摸交互及共存验收 | 实机验收通过（原方案用户确认；键盘补充配置/编译通过待实测） | 原验收保持；+/−、0、.、左、右五键下方新增到Y319，保留原布局/输入规则，待下载只验新增范围，随后接H7 |

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
| 10 | FT6336U SCL | **PB10 / I²C2_SCL AF4开漏**，100kHz，与OLED共线；400kHz/OLED停用试验已按用户要求撤销，原触摸偏移仍待解决 |
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

## 5. 当前软件迁移结果与阶段交接

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

**H6/H6.5原验收保持，H7现有四页功能已按本轮用户确认收口，当前A1.1见5.5。** 同步追加的数字键盘底行五键下方命中保留代码/构建及单项待实测记录，不能把本轮主流程确认外推到新增区域。应用层按固定9.6矩阵布局分键、原键值处理、无临时调试显示均保留；不重开旧触摸或H7主流程验收。以下各轮结果按历史证据保留。

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

#### H6.5续：2026-10-18点击仍难响应，重新开启圆点与OLED（诊断版编译通过，待实机数据）

- **用户反馈及授权**：当前触控非常不灵敏、点了仍无反应，明确要求继续开启调试小蓝点和OLED数据。该反馈说明稳定点击尚未验收；之前上下镜像的观察和修正保留，但不能将剩余难响应归因于方向或认定整体已修好。
- **本轮局部改动**：仅将`User/Hardware/FT6336U.h`的`FT6336U_OLED_DIAGNOSTICS`与`User/GUI/lvgl_port.h`的`GUI_TOUCH_TRACE`由0改1，并同步中文注释。恢复原有10像素青色小圆点（按下显示/松手隐藏、不拦截控件）及OLED上方5行诊断，保留底部按键提示；OLED只读取已有变量。映射仍为x=raw_y、y=319-raw_x，未修改方向/端点、总线速度、采样周期、UI或第三方源码。
- **实际验证**：原`cmake --build --preset build-debug`重新编译4个相关源文件并链接退出0，`build/Debug/h6_touch_debug_reenable_build.log`无编译warning/error/FAILED，RAM **105328B（80.36%）**、Flash **754724B（71.98%）**、CCM0。ELF含`touch_cursor`和`show_touch_diagnostics`，确认诊断已编入固件。现有`gui_touch_test`增量构建及`ctest -R '^gui_touch$'`单独1/1通过，日志`build/GUI_HostTest/h6_touch_debug_reenable_build.log`、`h6_touch_debug_reenable_test.log`；真实LVGL按下/松手可切四页，按钮内移动释放仍能点击，证明开启圆点未阻断该软件输入路径，不代表实机触摸已修复。未新增测试或重跑驱动/旧SPI套件，未下载/删除文件。
- **唯一实机动作与读数**：下载当前Debug诊断版并复位，在底部第二个页面入口按住约2秒后松手，观察圆点是否位于手指处、跟随移动且松手消失，并提供OLED照片或数值。R=原始坐标，XY=屏幕坐标，N/P=点数/驱动接受按下，S=HAL状态（0成功），ER/OUT=通信错误/坐标越界累计数，LP/LR/LC=LVGL按下/松手/点击累计数；比较操作前后变化，长按时LC不增加本身不表示故障，要看松手后。H6/H6.5保持进行中，下一轮从这批新实机数据接上，不提前进入H7。

#### H6.5续：2026-10-18触摸状态/点槽/跳点与导航局部修复（代码/编译通过，实机待复测）

- **本轮用户证据与授权**：用户报告之前上下颠倒时圆点/OLED位置可对应，现在圆点不出现或在手指附近跳，按下有时无数据、N/P与XY难对应，底部四按钮难点；明确要求仔细阅读并尝试修复。Y镜像本身不改变芯片点数；现有材料不能证明它导致所有新现象，也不能由主机测试认定实际电气噪声来源。
- **确定的诊断时序缺陷**：FT6336U_Read在取I²C2锁和HAL读取之前就清零全局N/P，此时可能阻塞等待OLED当前页。OLED任务逐字段取变量，就会显示旧XY配临时N0/P0，或混用两次采样。已改完整收帧/解析后在短临界区一起发布N/P、原始/屏幕坐标与事件；OLED每屏先复制静态快照，临界区不含I²C/绘图/等待，不增加OLED任务的大局部缓冲。松手/错误仍保留最后XY而P=0，这是有意设计；N/P不是触摸累计次数。
- **触摸算法加固**：一次读0x02～0x0E的13字节（点数+两个6字节点槽），只接受DOWN/CONTACT及有效编号/合法坐标。按编号跟踪主手指，N2不再直接松手、槽换位不换人；主手指离开而另一根仍在时先松手并等全部离开，防止另一根被接成新点击。三点中值抑制合法范围内的孤立跳点，新按下用当前坐标初始化历史，不等三帧；正常拖动通常有一帧跟随延迟。通信错误/非法帧/真实N0仍立即松手，不用延长按下或把越界坐标夹到边缘掩盖故障。filtered_count新增记录改变坐标的帧数，R为最新原始坐标、XY为滤波后送LVGL的值。寄存器格式对照Espressif官方FT6336U驱动，扫描全槽/编号模式交叉参考Linux edt-ft5x06驱动；这不代表所有实机故障已复现。
- **输入及按钮**：只把LVGL输入设备定时器目标从默认33ms设为15ms，显示刷新仍原周期，实际间隔继续看read_gap_ms；同GUI任务的绘图/阻塞SPI以及共用I²C等待仍可拉长间隔。四个导航按钮外观保持101×25，在四周额外4像素可命中；原按钮间距11像素，扩后不重叠，覆盖导航栏上下留白。所有点击仍来自LVGL真实命中/按下/松手，不直接发送CLICKED。两个诊断宏保持1，SWAP1/X镜像0/Y镜像1、端点、LCD参数、引脚、SPI/I²C时钟及共享锁均保留。
- **针对性验证**：扩展tests/touch_driver，旧版在N2保持按下的新回归实际失败（h6_touch_stability_before_test.log）。另外提取HEAD原驱动到build/H6_TouchTest/，独立发布时序用例在HAL替身读下一帧期间检查上帧N/P，旧版实际失败在N被清零（h6_touch_snapshot_before_test.log），新版同用例直接退出0（h6_touch_snapshot_after_test.log）。完整真实驱动目标1/1通过，h6_touch_stability_test.log；覆盖两槽/编号换位/主手指抬起/真实松手、孤立跳点/拖动/新触摸清历史及原错误/重试/复位/锁用例。真实LVGL输入目标gui_touch单独1/1通过，build/GUI_HostTest/h6_touch_stability_test.log，新增20ms短按与四个导航上下扩展区命中/切页，旧按钮内12像素移动测试保留。两套测试使用HAL/RTOS替身及主机时钟，不能充当实机波形/噪声/调度性能证据；未重跑旧SPI套件。
- **实际ARM构建**：原cmake --build --preset build-debug实际编译相关源并链接退出0，日志build/Debug/h6_touch_stability_build.log无编译warning/error/FAILED。RAM **105448B/128KiB（80.45%）**、Flash **755788B/1MiB（72.08%）**、CCM0；相对上一诊断版RAM+120B/Flash+1064B。ARM .su中show_touch_diagnostics静态栈24B、vOLED_Task16B、FT6336U_Read80B，不代表完整调用链或实机栈余量已验收。同步README及进度，未改CubeMX配置/第三方库/时钟，未删除文件、下载或取得本版实机结果。
- **下一步/验收边界**：下载当前Debug ELF复位，单指按住拖动、松手并逐个点四页入口。验圆点稳定跟手/松手消失、OLED同一屏N/P与坐标、四按钮容易命中；持续失败时给按住时完整OLED，比较ER/OUT及LP/LR/LC。当前H6/H6.5保持进行中，仅代码/构建及针对性主机验证通过，验收后才接H7，不恢复已取消的H5测试。

#### H6.5补充：2026-10-18短按偏移/拖动准确与400/415重复定位反馈

- **新用户观察**：短按时小蓝点偶尔在手指上方或下方，长按拖动位置准确；反复点同一个地方约出现400/415，400准、415偏下。用户称是X轴，但未确认数值是OLED的R还是XY/第几个数，亦未明确已下载上一轮修复ELF，不将二者自行推定。
- **只读核查**：FT6336U.c的filter_position首次把同一坐标填入三个历史位置，第一帧直接送LVGL；若首点本身偏移，该中值历史不能立即纠正它。纯数值输入400、415、400、415、400、415时现有三点中值输出400、400、400、415、400、415，说明不能把它说成已覆盖持续两档交替。未取得真实首帧序列，不能确定本次原始数据确为交替，或全部现象由算法引起。LVGL光标随输入点移动，应用未设置额外显示旋转；映射原始X0～319/Y0～479→屏幕X0～479/Y0～319，需先确认字段，不按“偏下/X轴”口述盲改方向或端点。
- **本轮处理与下一步**：用户随后明确400/415是OLED第二行XY第一个数，已定位为屏幕X。原始范围/映射使未滤波屏幕X=raw_y，三点中值只在最近三个值中取中间值，不能凭空产生新值；Y镜像只影响屏幕Y。下一步只比较P=1时第一行R第二个数是否同步跳，再核首触不动能否自行回准。回答尚不能判屏坏，先区分原始读回与历史滤波；仅实际更新本进度，无源码修改/新构建/下载/接线或测试结果，H6/H6.5保持进行中。

#### H6.5续：2026-10-18原始R也跳，修正未报告槽残留采信（代码/编译通过待实机复测）

- **本轮新证据**：用户在上一轮对照请求后明确“也会，会在00左右和二三十左右来回跳”，按其回答记录原始R也变化；保留原话，不自行补百位或认定是0/30、400/430。此前XY首数400/415及长按拖动准的反馈保留；没有完整报文、照片或波形，不能判屏坏，亦不声称获得内部寄存器锁帧证据。
- **再次核实的软件缺陷**：上一轮FT6336U_Read固定遍历两个槽，即使count=1也可能从未报告第二槽找回旧主手指ID，或在首槽为UP时把第二槽残留CONTACT/坐标当新按下。OLED的R是软件选中的坐标，因此R变化不能越过这层直接归因于芯片硬件。Espressif官方FT6336U驱动只按points读取/遍历记录；本轮保持一次连续收13字节，但只解析count覆盖的1/2个记录，不将未报告槽当有效数据。
- **更正旧测试假设及回归**：前次测试“N1、首槽UP、有效主手指仍在第二槽时应持续按下”缺少FT6336U官方驱动依据，本轮更正为N1只使用首记录，不把这一旧测试当成硬件协议事实。新增两种残留场景：主手指离开只剩另一人时第二槽残留旧主手指CONTACT，必须松手并等待全部离开；首槽UP而未报告第二槽含合法编号/CONTACT与(10,25)近零旧坐标，不得造按下或改变原始坐标。修正前真实运行的新测试失败在前一种场景，日志build/H6_TouchTest/h6_touch_report_count_before_test.log（line153）；仅将遍历上限2U改count后，完整真实驱动目标1/1通过，日志h6_touch_report_count_test.log。原四角/事件/滤波/错误/重试/总线/复位回归保持通过；这仍是HAL/RTOS替身软件证据，未复现实机全部跳动。
- **改动与构建**：只局部改FT6336U.c/.h的点槽解析上限及中文说明、对应驱动测试和README/本进度；保留Y镜像、端点、三点中值、新按下立即响应、输入15ms、导航外扩4像素、I²C2互斥和两个诊断开关，不改引脚/时钟/LVGL核心。原build-debug实际编译FT6336U/GUI_Task/OLED_Task并链接退出0，build/Debug/h6_touch_report_count_build.log无编译warning/error/FAILED，RAM105448B/Flash755788B/CCM0。本轮未重跑未修改的GUI/旧SPI目标，不把历史GUI通过说成本轮重新验证；未下载、删除文件或取得本次修正后的实机数据。
- **同步修改的保留与最终核对**：工作期间检测到tests/touch_driver的assert被同步替换为CHECK正常退出，并新增CMake失败输出自检脚本；这部分不是本轮编写，已保留，没有覆盖或删除。随后按当前保存文件运行构建（no work）及`ctest -R '^touch_driver$'`，驱动目标单独1/1通过，日志h6_touch_report_count_current_build.log/current_test.log。未把新增失败自检当成本轮已执行，也不将之前assert失败的历史行号改成当前CHECK版本行号。
- **下一步**：用户下载当前Debug ELF复位，同一点重复短按对照R/XY和圆点稳定性；仍异常给按下P=1时完整OLED照片/短视频，核完整数值及S/ER/OUT。这个修改修复可复现的软件错误，但不能保证消除实机所有初触/持续噪声；H6/H6.5继续进行中，实机稳定交互后接H7，不恢复H5取消测试。

#### H6.5照片证据：2026-10-18原始X变化对应屏幕Y偏移（进行中）

用户提供三张视频截图，明确第一张准、第二/第三偏。实际以view_image查看原尺寸原图，以下只记录清楚可读的数据，不把标签当作已测得真实手指像素位置。

| 图片 | 原始R(x,y) | 屏幕XY(x,y) | N/P、HAL状态及错误 | 事件LP/LR/LC |
|---|---|---|---|---|
| 1（用户确认准） | (16,433) | (433,303) | N1/P1，S0，ER000000 | 78/77/74 |
| 2（用户确认偏） | (3,433) | (433,316) | N1/P1，S0，ER000000 | 82/82/79 |
| 3（用户确认偏） | (51,431) | (431,264) | N1/P1，S0，ER000000 | 88/87/84 |

- **原图位置**：均在`D:/Temp/QQ/Documents/Tencent Files/3240342080/nt_qq/nt_data/Pic/2026-10/Ori/`，依次为`de229e05ddb5dadcc01dca36b6441da6.png`、`eddda288466479b9af03a8619a108119.png`、`6448ade20c78bd3efc645b001ba3e6f7.png`。未搬动/编辑/删除用户图片。第一、二张OUT00000、ID64/RD1可清楚读出；第三张该行局部缺损，不由此推断通信故障。
- **对旧口述方向的更正**：此前根据用户选择聚焦屏幕X/raw_y；这组三图却显示屏幕X只在431～433变化，主要变化在原始X16/3/51与屏幕Y303/316/264，后者范围52像素，应按照片更新排查轴。当前未滤波映射y=319-raw_x，前两张直接对应；第三张319-51=268而输出264，与三点中值依赖历史的设计相容，没有真实前几帧，不能另判映射算错。Y镜像只改变变化方向，不能独立造成原始R的变化。
- **能支持/不能支持的结论**：按用户固定触点的上下文，存在触摸坐标重复性偏差；变化已出现在软件选择/解析后的原始坐标层，故排查采样/读回环节。S0/ER0表示驱动没有记录到HAL失败，不证明每个数据字节或实际触点正确；N1/P1仅说明该帧按下被接受。照片只包含OLED/核心板及底板局部，没有触摸面板/手指、电源波形或总线报文，不能确认面板本体损坏、供电/环境噪声或读取软件中的哪项。板上ELF版本也无法独立由照片核实。
- **下一小步**：三图XY均接近逻辑屏幕下边缘，先保持固件/接线/电源不变，在屏幕正中央同一位置垂直短按5次、不拖动，比较R第一个数是否仍相差几十。中央明显稳定才继续查边缘/落指相关因素；中央同样明显跳再隔离全屏采样/电气环境与软件读回，不能仅凭此判屏坏。ST官方AN4299指出电容触摸系统可受传导噪声影响，这是通用机制依据，不是FT6336U本机噪声测量结论。
- **本轮边界**：仅只读核对源码/映射、图片和官方通用资料并实际更新本进度；没有改工程源码、增加测试、重编译、下载、改线或标H6实机通过。上一轮按N限槽修复、ARM/驱动证据及旁支CHECK失败输出日志保留；不覆盖并行任务所追加的进度。

#### H6.5算法检查：2026-10-18中央稳定、上下边缘偏移（进行中）

- **新增实机反馈**：用户按上一轮要求做中央对照，R第一数稳定，没有相差几十；问题仅在顶部/底部，偶尔上下跳，但多数小蓝点靠近边框。随后回答文字澄清：“蓝点更靠近，而且会上下波动，请查看process里面应该有说明”，据此明确圆点比真实触点更向边框偏，排除仅仅是手指本就在最外沿的语义解释；尚无这轮精确触点/连续帧/板上ELF版本，不自行补偏差像素或推定只发生在初触。
- **源码检查**：map_point对所有合法点统一交换/镜像，当前端点使x=raw_y、y=319-raw_x，越界拒绝而非夹到边缘，没有顶部/底部额外分支；无额外显示旋转。10像素圆点横/竖均平移-5，以触摸坐标作为圆心，最边缘可被裁切，不能解释几十像素的坐标偏差。三点中值每次松手清历史，稳定输入不产生新坐标；首帧复制成3份，首点偏移会立即通过，下一帧正确也仍可能输出旧位置；持续偏差无法用取时间中值纠正。这里区分算法性质和实物根因，未判屏坏或认定需要哪种标定。
- **澄清后的算法判断**：已有文档同时记录首点偏移、连续两档变化不能被当前中值消除、三图原始X16/3/51与中央稳定。用户这次明确“向框偏＋上下波动”，需要分别保留空间对应误差与时间跳动的排查，不能把首点限制说成全部根因，也不能把320个指定输入测试通过当作面板标定正确。尚无已知物理位置的连续原始帧，无法在不猜参数的前提下求校正系数；本次不修改方向、原始端点或加重滤波，不重新要求用户解释“靠边框”的含义或重复中央测试。
- **电脑验证**：在build/H6_TouchTest/h6_edge_algorithm_probe.c复用已有HAL/报文替身并另加总线替身，直接编译真实FT6336U.c，MinGW GCC13.1使用-Wall/-Wextra/-Werror，编译及运行退出0。320个合法raw_x逐个松手后读4帧均稳定输出(240,319-raw_x)；上侧(raw_x299)及下侧(raw_x20)各注入一个合法边缘跳点(319/0)，滤波输出保持稳定。首触指定raw_x=3/16/16输出Y=316/316/303，验证首点会通过且延续至下一帧；这不是用户真实首帧序列。日志h6_edge_algorithm_probe_build.log、h6_edge_algorithm_probe.log，不称为完整旧回归或实机复现。
- **同步工作边界**：检查期间I2C2_Bus、FT传输上报、OLED/GUI任务和测试替身被其他工作同步修改。首次直接联编原总线因旧替身缺GPIOB等恢复接口而失败；随后本探测仅以锁/结果上报替身隔离总线，未覆盖或删除任何同步源码。首次失败及最终边界在此记录，最终探测成功不证明真实I²C恢复已通过。未构建ARM/下载/改方向、端点、时钟或滤波，也未修改正式测试源码。
- **下一步单项操作**：保持现有固件，在同一竖线分别距上/下边框约20像素的内侧各按住1秒、不拖动，记录稳定R第一数/XY第二数，以及按住后是否自行回准。理想对照上侧约299/20、下侧约20/299；人工约20像素仅供识别明显外偏，不作为精确标定。持续偏差时再据实际触点核标定，只有初触偏移时再设计首点处理并评估短按漏采代价。中央测试已完成不重复，H6.5保持进行中。

#### H6.5续：2026-10-18测量完成后触摸/OLED同时停更（恢复效果已获用户确认，初始触发仍未确定）

- **后续实机验收更新**：用户明确“不会卡死了”，因此恢复效果按反馈收口，不再要求重复原三次测量测试。下列原现象/实现/构建及当时待验条件作为历史保留，不能据此认定具体原始电气故障或边缘问题同因。

- **反馈与范围**：用户确认故障在显示“测量完成”时发生，OLED OK/底部按键提示停止，但实体KEY1/KEY2仍能切液晶四页。GUI仍运行，共享I²C2为优先范围；尚无故障寄存器/线路证据，不能认定控制器BUSY、物理拉低或与边缘偏差同因。
- **应用回归**：完成路径只暂停自身模拟测量timer，没有禁用输入；新增真实指针标定→开始→完成→继续采样/导航，连续3轮及lv_mem_test通过，gui_touch单独1/1（build/GUI_HostTest/h6_measure_complete_touch_test.log）。此替身不模拟实际I²C、供电或接收时序，不能排除板上问题。
- **修复的处理缺口**：本地HAL I2C_WaitOnFlagUntilTimeout及部分收发超时路径只恢复软件READY并返回，并不保证发STOP、释放硬件事务；原共享层只有锁，FT每秒读ID/重配也不会释放卡住的线路。现OLED/FT在HAL返回、保留原错误后仍持锁上报；忙/超时等控制器故障每秒最多恢复一次：DeInit/复位I²C2→PB10/PB11开漏释放→SDA低则最多9个数据时钟→STOP→归还AF4/原100kHz配置。SCL/SDA持续低或Init失败均有限退出；取锁失败/单纯不应答不重置，不重发坏帧冒充成功，触摸仍松手。未改CubeMX非USER CODE区/引脚/频率/方向/滤波/第三方库。
- **可见证据及栈**：g_i2c2_bus记录最近失败地址/状态/HAL错误/SR1/SR2/SCL-SDA、锁超时和恢复计数；GUI_TOUCH_TRACE开启且传输/锁失败后，独立SPI液晶每秒显示I2C E/K/R/A/S/L，不依赖失效OLED。E=传输失败，K=锁超时，R=线路/控制器空闲恢复成功/尝试，A=七位地址38/3C，S=HAL状态，L位1=SCL/位0=SDA（3=均高）。故障记录可被后续另一设备的失败更新；R成功不等于触摸精度或根因确认。OLED栈128→256个32位字，运行时堆多用512B（静态堆容量不变）；新增ReportResult/recover_bus静态帧32/40B，不充当任务栈实测。
- **验证**：真实总线实现+电平模型覆盖硬件BUSY残留、SDA第3补时钟释放、STOP/AF归还、SDA/SCL持续低有界退出、Init失败、每秒限次、NACK/启动期不复位、锁竞争不复位及错误留存；与触摸数据回归和旁支失败退出自检3/3通过，build/H6_TouchTest/h6_i2c_recovery_build.log、h6_i2c_recovery_test.log。ARM初次实际编译6个源，最终字段调整后再编GUI_Task并链接，均退出0，最终RAM105512B/Flash757660B/CCM0，无编译warning/error，build/Debug/h6_i2c_recovery_build.log、h6_i2c_recovery_final_build.log。未下载、未测波形/实机恢复、未删除文件；同步边缘算法检查/既有改动保留。
- **依据与边界**：[NXP UM10204 3.1.16](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)给出SDA卡低补时钟的方法；STOP另有一次SCL上升，不是第10个数据补时钟。[ST ES0182 2.10](https://www.st.com/resource/en/errata_sheet/es0182-stm32f405407xx-and-stm32f415417xx-device-errata-stmicroelectronics.pdf)列有I²C时序/错STOP限制，但没有板上波形，不能写成已触发具体芯片勘误，仍保留100kHz。
- **下一步**：下载当前Debug并复位/重新模拟标定，连续测量3次，完成后触摸切页、确认OLED OK/底部KEY提示仍更新；异常拍液晶I2C条+OLED，没有条也反馈。先验证此问题，再接上下边缘内侧稳定点对照，中央稳定已记录勿重复；H6/H6.5仍进行中。

#### H6.5续：2026-10-18依据旧三图修复边缘首次落点/上下跳动（代码/编译通过待实测）

- **授权/证据**：用户确认已不再卡死，中央R第一数稳定，仅上下边缘向框偏且偶发波动，明确要求修复；进一步说明不方便补两组数据，要求使用前面三图。本轮不再索要同组数据或重复中央测试。旧图屏幕Y303准、316/264偏，同点向两侧变化，不足以支持全屏统一平移或空间校准系数。
- **驱动历史修复**：User/Hardware/FT6336U.c/.h定义上下64像素为首次确认区域（覆盖三图Y303/316/264，不是偏移系数）；此处首次不复制三份相同坐标，1/2/3个真实样本逐步填入，两个样本时暂用最新真实点，满三点才取中值。中央仍即时响应、原初始化保留；N限槽/ID/错误松手、原始范围/镜像/端点及共享总线都未改。
- **GUI确认与短按**：lvgl_port.c/.h先等边缘两点各轴差≤8像素，或第三个真实样本的中值再认定按下；持续按住时边缘Y跳变>8先等一帧，回到原位置的孤立/交替跳点不跟随，连续新位置或继续同向移动则接受，避免锁住快速拖动。64/8是时间确认范围/阈值，不是空间修正，不裁去Y0/319。仅一/两帧就真实松手的短按仍保留：先提交已观察到的按下，continue_reading让LVGL同轮消费其已知释放，不手工发CLICKED、不额外读I²C、不延长按钮按住状态；读失败/非法坐标取消待确认点。短按圆点额外留80ms视觉提示，常规触摸松手即隐藏。
- **诊断对齐**：OLED XY现在读取g_gui_touch.x/y，反映最终交给LVGL/圆点的位置；R仍原始，N/P仍硬件驱动检测，确认期间可先N1/P1、XY沿旧点而LP未增。g_gui_touch.pending/edge_waits/edge_holds/short_taps记录确认等待/大跳确认/短按补交；FT内部中值结果仍在g_ft6336u.x/y。避免将中间驱动值与最终光标混为同一层。
- **验证**：驱动新增rawX3/16/16期望Y316/303/303，回归上一版首点副本会输出316/316/303的限制；320个合法rawX逐个4帧稳定且无端点改动。驱动/真实总线恢复/失败退出自检3/3通过（build/H6_TouchTest/h6_edge_settle_build.log、h6_edge_settle_test.log）。真实LVGL输入1/1，覆盖照片数值构造的316/303/303和303/316/264、两档交替、顶部对称、稳定0/319、中央立即响应、真实移位/快速同向拖动、错误取消、单样本短按恰好一次点击/立即松手/不多读硬件、80ms圆点及原20ms/四导航/三次测量完成后交互（build/GUI_HostTest/h6_edge_settle_final_build.log、h6_edge_settle_final_test.log）。图中数值仅构造测试输入，不称实机连续帧复现。
- **ARM及边界**：原cmake --build --preset build-debug实际编FT6336U/lvgl_port/OLED_Task及头依赖GUI_Task并链接退出0，RAM105600B/Flash758764B/CCM0，无编译warning/error，日志build/Debug/h6_edge_settle_build.log。没有改I²C/SPI时钟、引脚、第三方库或已恢复的总线机制，未删除文件/下载/实测。持续稳定的空间偏差及单样本自身偏错不能由时间确认还原；本轮没有足够已知物理位置拟合全屏校正，不能认定全部边缘外偏已解决或屏坏。
- **下次唯一验收**：下载当前Debug复位，照之前方法点击顶部/底部和四导航，直接反馈蓝点/按钮响应是否改善及拖动/松手是否正常；不用再抄新参数、补同组三图或重做中央对照。H6/H6.5进行中，稳定交互通过后接H7；若仍持续空间偏差，后续如需精确校准须先说明所缺对应信息，不盲调端点。

#### H6.5续：2026-10-18底部按钮处R=(0000,0177)按住不变，偏移仍在

- **用户反馈**：提供原始“0000 0177，还是偏”；进一步明确手指在底部按钮位置，按住后没有变化。按其上下文记录底部持续定位偏差，不推定瞬时抖动已全部改善、已下载最新ELF或本次N/P/OK值。
- **源码复核**：OLED R直接取g_ft6336u.raw_x/raw_y；FT按报告N和有效事件/编号选择记录，raw_x=((XH&0x0F)<<8)|XL，在map_point/滤波前赋值。原始(0,177)基础换算(177,319)，不存在把负值/越界值夹成0的分支。松手/无效帧会保留旧原始值，因此没有本次N/P/OK时仍不能绝对排除保留值或失效读回。导航板Y280、高33，按钮相对Y4、高25，主体约Y284～308、中心296；有效坐标Y319在实际主体之外，符合偏到框而难命中的现象，不将手指在按钮说成触摸最外边。
- **结论边界**：若该值确为按住时实时有效读数，异常已经位于原始读回/触摸端，更怀疑触摸模组边缘定位、配置/电气或通信读回；时间滤波无法把持续0恢复为真实按钮位置。没有隔离I²C/供电或取得连续原寄存器证据，不能直接判面板物理损坏或要求换屏。本轮不盲调端点、裁边、把0固定映射到按钮，也不继续堆滤波。
- **实际动作/续接**：只读检查与本进度更新，未改业务源码、增加测试、构建、下载或删除文件。最近ARM与主机通过作为旧证据保留；H6/H6.5继续进行中、不再卡死按用户反馈收口，下一方向为原始读回与触摸模组隔离，避免重复索要其不方便提供的坐标/图片。

#### H6.5续：2026-10-18商家源码对照修正、撤回多层算法（配置/编译通过待实测）

- **用户授权/读取依据**：用户明确要求停止讨论并读取`D:/.Data/spi 3.5寸屏幕资料/CL35BC1070-40A_CTP`修复、认为之前叠加过多。实际以GBK解码完整阅读F1_4SPI的`HARDWARE/TOUCH/ctp.c/.h`、`ctiic.c`，核STC32_4SPI同算法及F1 `LCD_Display_Dir/LCD_Init`、`IC&Code/ST7796_3.5IPS_Code.txt`；原商家资料只读，未修改/编译/执行其脚本。主要源码为[商家ctp.c](<D:/.Data/spi 3.5寸屏幕资料/CL35BC1070-40A_CTP/测试程序/STM32F1_ST7796_IPS_4SPI_CTP Demo_V1/HARDWARE/TOUCH/ctp.c>)。
- **可核实差异**：商家硬复位20ms/释放等300ms，不写DEVICE_MODE/G_MODE；扫描先读0x02的1字节点数，再从0x03起读4字节坐标，明确仅`(buf[0]&0xF0)==0x80` CONTACT有效，原始双零忽略；横屏交换轴。此前本地接受DOWN、写模式并加入中值/边缘确认等，先修软件与商家的差异，不能继续提前归咎手指或硬件。商家LCD横屏dir3为MADCTL0x28且启INVON，与本项目显示实测基线一致；其示例LCD_Init与独立txt电源/gamma参数自身也有差别，未混抄这些参数来猜触摸问题。
- **当前实现**：FT改为点数1+坐标4字节、只接受CONTACT，去模式写入、中值历史、边缘策略、跨帧编号跟踪；本GUI只提供单指，N2不按下且不扫其他槽。双零拒绝、单轴0保留合法边缘；不把0强行变成按钮坐标。额外只读芯片/固件/厂商身份供诊断，错误/重试/范围拒绝/发布快照保留。两个HAL事务持同一I2C2锁，保留F407 HAL和已收口的不再卡死恢复，不搬F1/STC引脚、软件I²C或按调用次数降采样。已确认方向x=raw_y/y=319-raw_x、15ms输入目标、时钟和LCD参数不变。
- **GUI/诊断简化**：lvgl_port只直接转交有效点/松手，删除二次确认、8像素跳变策略、短按补交、continue_reading循环和80ms圆点余辉；P无效/松手即时隐藏光标。删去对应多余状态/计数，OLED XY仍最终界面点，R原始；首行新增已有事件E显示（0 DOWN/1 UP/2 CONTACT/3无记录），只有有效单指E2才P1，R可在初始DOWN时变化但不会画偏点。仍保留蓝点/OLED诊断，未删除任何文件或覆盖旁支CHECK失败报告修复。
- **验证证据**：先加商家契约在旧实现运行，实际失败于模式寄存器被改写（build/H6_TouchTest/h6_vendor_before_build.log、h6_vendor_before_test.log，退出1），不是实机根因复现。当前驱动严格检查0x02/1字节和0x03/4字节、同一取锁、DOWN拒绝/CONTACT更新、保留最后点、双零/多指拒绝、第二笔坐标读失败解锁/原错误保留、320个原始X直接响应及照片值无历史改写；正常驱动/总线恢复/失败报告3/3通过，独立`--vendor-contract`退出0（h6_vendor_build.log、h6_vendor_test.log、h6_vendor_contract_test.log）。原中值/边缘/多指跟踪测试随撤回功能改为实际商家契约，不把删除算法当作旧算法仍通过。真实LVGL输入直接逐帧转交、单帧有效点点击/立即松手、20ms有效点/四导航、标定及3次模拟测量完成后继续输入1/1（build/GUI_HostTest/h6_vendor_build.log、h6_vendor_test.log）；有效点替身不证明极短物理点击一定已产生CONTACT，没有CONTACT不人为补点击。
- **构建/边界**：ARM原build-debug实际编FT/lvgl_port/GUI_Task/OLED_Task并链接，OLED事件字段调整后再编该源并链接，均退出0；最终RAM105504B/Flash757140B/CCM0、无编译warning/error，日志build/Debug/h6_vendor_touch_build.log、h6_vendor_touch_final_build.log。未下载/亲测；这确认源码/编译及协议差异已改，不证明实物全部偏移已修或屏幕正常。之前原始(0,177)未带事件值，不能倒推它一定是DOWN或CONTACT。
- **唯一下一步**：下载当前Debug复位，直接按原方式点顶部/底部和四导航、拖动松手，反馈是否改善；不重复两组新参数、中央对照或已收口的卡死测试。H6/H6.5继续进行中，稳定后接H7，不恢复H5取消测试。

#### H6.5协议审计：2026-10-18简化版略灵敏但仍偏移（未改固件）

- **实机反馈**：用户确认响应略改善、位置仍偏，要求再次严格核商家命令/识别并保持简洁；H6.5未通过，不把上一轮编译/主机通过当作偏移解决。
- **地址与读取**：商家0x70/0x71是写/读地址字节，本地HAL用0x38左移一位，I2C_RequestMemoryRead自行发写地址、8位寄存器、重复起始、读地址；不是把读操作误发成写。先读02点数，再读03～06，12位坐标只取XH/YH低4位，高字节在前；YH高4位手指编号已剔除。
- **事件与方向**：XH位7:6分别00 DOWN、01 UP、10 CONTACT、11保留，位5:4保留；本地按商家完整高半字节0x80检查。CONTACT是持续接触事件，不保证坐标准确或已校准。商家电容分支不走touch.c电阻屏校准；本地x=raw_y/y=319-raw_x是已有实机方向适配，GUI没有二次旋转/边缘缩放。
- **直接对照**：分别原样复制F1/STC32_4SPI ctp.c并编译，替换引脚/软件I²C为可控寄存器，与当前真实FT6336U.c和I2C2_Bus.c同一电脑可执行文件中比较；仅对商家输出做本机方向归一。每套1819136组（320个合法原始X和3个越界X、11个Y、16种XH事件/保留半字节、16种YH编号、商家两种方向），按下和有效坐标均一致。F1 SHA256 ED2EE98EA4287AA4BBC1E9BF8335DE6032AAAE244E33E13A0042BDB4D5131030；STC SHA256 A8ECF1E76BB3155FB243CB65160F710ECFADD3A468959D7531FD2A88A773373C。构建/结果在build/H6_TouchTest/vendor_audit/{build,test,stc_build,stc_test}.log，两次编译/运行退出0；首次工具调用缺MinGW运行路径未生成程序，补进程PATH后才成功，不冒称首试通过。
- **保留差异和边界**：本地单指N2松手、HAL/RTOS锁/超时恢复、只读身份诊断及15ms目标；商家多点/裸机计次降采样未整份照搬。审计没有真实I²C波形或物理触点，不能证明面板正常/损坏；未改业务源码、未加补偿/滤波、未重建ARM、未下载或删除文件。下一步实物读回/模组对照；不让用户重刷同一版或重复已拒绝的坐标采集。

#### H6.5隔离试验历史：2026-10-18停用OLED、I²C2提高至400kHz（已按用户要求撤销）

- **用户授权和改动**：用户明确“禁用OLED，提高I2C速率试试”。OLED_Task.h加一个OLED_TASK_ENABLED=0编译开关；main的USER CODE区跳过OLED开机探测/初始化/刷新、专用信号量和任务创建，不删除文件。保留总线锁/恢复、GUI/按键/蜂鸣器、蓝点与商家CONTACT解析，未加算法。
- **速率及生成影响**：Core/Src/i2c.c的ClockSpeed由100000改400000，.ioc I2C2.ClockSpeed同步400000，DutyCycle保持2，由现有HAL生成快速模式参数；USER CODE注释说明用途。该行位于生成区但与.ioc一致，后续CubeMX生成应保留400kHz。恢复路径仍HAL_I2C_Init使用hi2c2.Init，只修过时100kHz说明，不重置为旧速率。未改引脚、上拉、接线、SPI速率或LVGL。
- **验证证据**：原build-debug首次实际编main/i2c/OLED_Task并链接，注释同步后再编I2C2_Bus并链接，两次退出0；最终日志build/Debug/h6_oled_off_400k_build.log无编译warning/error，RAM104360B/Flash749292B/CCM0。nm日志无OLED_*、vOLED_Task和show_touch_diagnostics，触摸/GUI/蓝点/总线锁符号仍在；反汇编确认ClockSpeed=0x61A80=400000。未新增测试来重复配置，旧主机回归未重跑；未下载/测波形/实物性能，不宣称偏移已解决。
- **下一步与边界**：下载当前Debug并复位，沿原操作对比上下边缘和四导航点击/拖动/松手，直接反馈响应/偏移变化。OLED没有收到关屏命令，单独复位MCU时旧画面可能残留；这是停止访问的预期表现，不能据此判断任务仍运行。原OLED仍物理接在总线上，本试验只停软件事务并提高设定频率；两项同时改变不能分别证明根因，不要求重复中央或抄OLED数据。H6整体进行中，试验配置/编译通过待实测。

#### H6.5撤销：2026-10-18恢复OLED与100kHz（回退构建通过）

用户明确撤销紧邻上一轮的OLED停用/400kHz试验。已移除OLED_Task.h试验开关及main四处条件编译，恢复原OLED启动/信号量/任务；i2c.c与.ioc回到100000，恢复试验前总线注释。未撤销更早的商家触摸简化、蓝点或共享总线防卡死修复，未删除文件。README/当前进度已同步，旧试验日志保留为历史，没有收到其实际效果反馈。

原build-debug实际编4源并链接退出0、无编译warning/error，RAM105504B/Flash757140B/CCM0，与试验前相同；ELF恢复OLED初始化/刷新/任务函数，触摸/蓝点仍在，反汇编确认100000。日志build/Debug/h6_restore_oled_100k_build.log及对应symbols/i2c_disassembly.log。未下载/实测，不认定偏移修复；本轮撤销完成，若板上曾下载试验版可下载当前Debug恢复，否则无需重复下载，等待用户下一指示。

#### H6.5商家回复：2026-10-18通用固件、定制调校与无效坐标（仅解释）

- **用户请求及证据**：用户提供`8f41b4eab0b8f4f44da63b0c36bc78e4.png`与`a76994fea6fae19d74d2e1bf7443cd64.png`两张商家聊天截图，要求理解含义。商家称使用通用电容触摸固件、批量产品可寄整套设备定制升级；对毕设建议主控软件排除偏离坐标，并称内部固件只有芯片厂家有权限。截图是待分析的商家陈述，不作为用户要求改代码或联系商家的指令。
- **两层软件**：商家语境中的固件指触摸控制器内部程序及调校参数，区别于F407上的`FT6336U.c`、商家`ctp.c`等读取坐标的主控驱动。厂家内部程序、工具与调校资料是否能提供须以实际支持为准；不能把“只有厂家有权限”扩大为STM32驱动不能修改。[FocalTech FT6336U数据手册](https://files.waveshare.com/wiki/common/FT6336U_Datasheet.pdf)描述芯片内MCU/Flash保存程序和参数并经I²C与主控交换数据，支持这一层次区别。
- **解释边界**：“未适配UI”说法笼统，触摸芯片报坐标、主控界面决定命中哪个按钮；不能据此认定每种LVGL按钮布局都要定制内部固件，或本机问题已证实由此导致。商家没有提供阈值、实现、调校前后数据，也没有明确证实手指、硬件损坏或换笔能解决。
- **软件筛选的适用范围**：偶发远离前后稳定位置的点可评估排除，但必须兼顾真实拖动和新按下。旧图Y303准而316/264偏只作已有样本，不能自行视为连续帧；先前底部按住原始X持续0若为实时有效数据，对应屏幕Y319，单纯丢点无法还原真实触点，可能使按钮更难响应。319是合法边缘坐标，不能一律判无效；也不能按想点的按钮强行吸附坐标。
- **本轮实际动作与续接**：完整读取规则/进度及git状态，只读复核当前CONTACT解析、坐标映射、.ioc/i2c.c的100kHz及根CMake/LVGL配置，保留所有已有改动；仅修改本进度，无代码/测试/构建/下载/新增实机证据。H6/H6.5保持进行中；后续若实施软件处理，先明确异常点判据及持续偏差边界，不直接恢复旧多层算法或重复已取消采数任务。

#### H6.5续：2026-10-18按钮触摸范围扩展（配置/编译通过待实测）

- **用户请求**：底部四个LVGL按键保留原判定位置，并把各键下方空白纳入判定；其他按钮适当扩大，以容忍触摸偏移。本次是UI命中范围调整，不是筛选坐标或空间标定，不据此认定原定位偏差已修复。
- **底部四键**：`measurement_page.c`保留101×25、原绝对坐标X=13+112×序号/Y284，左右/上方各4像素，向下覆盖到Y319。按钮改归整屏root管理，原33像素高导航背景原样保留；否则LVGL搜索子对象时会被旧背景Y312的边界截断。外框先扩11像素，再以HIT_TEST限制横向和上方，四键不会因向下扩展而横向重叠。
- **其他按钮**：开始测量四周最多外扩8像素、保留结果面板边界；`instrument_ui.c`的统一按钮工厂为返回/操作/翻页/设置行/记录行/确认取消等外扩8像素，并读取当前可见可点击兄弟按钮的位置，以间隙中线划分，2像素行距各用1像素。隐藏项不参与、禁用按钮保留自己的区域；仍由LVGL处理正常按下/松手/禁用和弹窗遮挡。数字键盘是按钮矩阵，核心已有按键间隙的点击扩展，沿用其逐键逻辑。
- **版本适配与范围**：两处命中回调包含固定LVGL9.6的`src/core/lv_obj_event_private.h`以取得`lv_hit_test_info_t`完整定义；不改第三方核心，仅比较矩形坐标并写命中结果。首次编译因未包含该结构定义及使用未公开的点判定函数而失败，日志h6_hit_area_build.log保留；改为包含对应头、显式坐标比较后通过。没有改FT6336U、lvgl_port、原始/映射坐标、CONTACT、采样、时钟/引脚、OLED或总线恢复，未恢复历史滤波，未删除文件。
- **实际验证**：扩展现有`tests/gui_migration/gui_touch_test.c`，运行真实LVGL/输入定时器及四页面，检查每个按钮下方到Y319所有像素和横向4像素边界、底边实际按下/释放、按住后从主体偏到底边、密集行分界、外扩处启动模拟测量/记录/设置/返回/取消、禁用限制、确认及编辑层遮挡。原20ms/按钮内12像素移动/直接输入/三次测量后继续交互回归保留。MinGW -Wall/-Wextra/-Werror构建退出0，gui_touch单独1/1通过，最终日志`build/GUI_HostTest/h6_hit_area_extended_build.log`、`h6_hit_area_extended_test.log`；不称旧SPI/全部主机套件通过。
- **ARM与续接**：原`cmake --build --preset build-debug`实际编2源并链接退出0，最终日志`build/Debug/h6_hit_area_final_build.log`无编译warning/error/FAILED；RAM105504B/128KiB、Flash757852B/1MiB、CCM0（静态RAM不变，Flash比上轮+712B；运行时LVGL池余量未实机测量）。未下载/亲测。H6/H6.5保持进行中；下一步只下载当前Debug复位，点击四键下方空白和页面按钮边缘，反馈是否容易命中、是否串键，无需重复中央/采数或已收口的卡死测试；稳定实机交互通过后接H7。

#### H6.5收口：2026-10-18用户确认验收、清理调试功能

- **实机依据**：用户明确“H6.5验收完成”，接受当前底部四键到底边、其他独立按钮最多8像素且按邻键分界的方案。H6.2～H6.5按功能验收收口，H6整体记实机验收通过；不据此推定原始坐标偏差的物理根因或重新要求旧诊断材料。下一阶段H7，尚未开始新业务实现。
- **实际清理**：删除GUI_Task的SPI液晶I²C调试条和每秒栈堆监测，删除lvgl_port的光标创建/绑定/显隐、事件追踪和输入/刷新统计及对应公开类型/宏。删除OLED_Task的触摸快照和R/XY/N/P/E/OK/ER/OUT/LP/LR/LC绘制；保留原OLED启动及KEY提示。删除FT诊断快照/INT观测/采样间隔/计数和固件/厂商号查询，只保留ID核验、私有就绪/重试/最后坐标。删除I2C2公开诊断统计和SPI调试计数；共享总线限次恢复/互斥与SPI完成结果/ISR信号量/DMA安全收尾保留。LVGL内存断言/错误停机属于运行保护，继续保留；按钮命中代码未改。
- **针对性回归**：原GUI测试改从LVGL公开输入状态/坐标观察，事件计数仅存在主机测试内；验证无绑定光标/系统层调试对象，并保留四页、20ms短按/12像素移动、底边全像素和按住偏移、外扩测量/设置/返回/取消、密集行分界、禁用/弹窗遮挡和三次测量后继续输入。gui_touch单独1/1通过（build/GUI_HostTest/h6_debug_cleanup_build.log、test.log）。驱动/总线测试改用返回点、事务次数和HAL/GPIO模型，删除仅针对已撤回调试快照/计数的断言，保留坐标/CONTACT/双零/多指/非法数据/错误松手/锁/重试/复位/ID及总线卡低有界恢复；触摸/恢复/失败输出3/3通过（build/H6_TouchTest/h6_debug_cleanup_build.log、test.log）。不称无关旧SPI套件已重跑。
- **ARM/符号证据**：原cmake --build --preset build-debug实际编9源并链接退出0，build/Debug/h6_debug_cleanup_build.log无编译warning/error/FAILED，RAM105176B/Flash753540B/CCM0，比上轮RAM少328B、Flash少4312B。ELF无show_bus_fault/show_touch_diagnostics/touch_cursor/trace_touch_event及g_gui/g_gui_touch/g_gui_display/g_ft6336u/g_i2c2_bus/g_spi1_*调试符号；保留FT6336U_Read、navigation_hit_test、button_hit_test、recover_bus、OLED_Update/vOLED_Task、SPI1_Bus_TransmitDMA和两个HAL回调，日志h6_debug_cleanup_symbols.log。未下载/亲测/更改接线时钟/修改第三方库或删除文件，保留所有原工作区改动。
- **续接**：H6.5实机通过来自用户确认；本次清理版仅有代码/构建/软件回归证据。下载当前Debug复位后调试界面消失；不因此重新打开H6.5验收，下一轮从H7四页面整体验收接上。

#### H6.5补充：2026-10-18数字键盘底行五键向下命中（配置/编译通过待实测）

- **用户请求/范围**：在已确认H6.5完成之后，补充要求数字键盘底行+/−、0、.、左、右五键像四导航一样把下方延伸到底边。保留原验收，不重开触摸根因排障；本轮只做此应用层补充。
- **实际实现**：keyboard仍位于(7,121)、466×192，外观到底Y312；新增7像素命中外框，用HIT_TEST限制仅Y313～319且在原键盘宽度内。固定LVGL9.6未公开单键坐标，仅读取buttonmatrix_private.h的实际布局，用底行相邻键间隙中线划分横向归属，不修改矩阵布局/第三方库。原键盘本体及上三排保持默认处理。矩阵先处理真实按下后，应用仅为新增区选中对应键并发送正常VALUE_CHANGED，仍由原键盘处理字符和光标，不发送CLICKED、不改触摸坐标或复制输入算法。一个私有键号保存按住状态，偏到底边仍属同键、横移到另一列取消，原长按重复继续工作。单键隐藏/禁用仍限制新增区。正负号沿用原accepted_chars和参数取值规则，没有因扩大命中而新增负数输入能力。
- **真实GUI回归**：扩展现有gui_touch测试，五键逐个比较可见键位与其下方每一行直到Y319的文字/光标结果，必须各只发一次对应键值事件；检查相邻分界两侧、上三排、顶部/左右不额外扩展、禁用0、底边长按重复、按住0偏到底边及横滑点号列后取消、原收起键、矩阵17个可见键区域字节级保持。保留原四页/20ms/按钮内移动/底边导航/外扩操作/禁用弹窗/三次测量后输入回归。MinGW -Wall/-Wextra/-Werror构建通过，gui_touch单独1/1通过，最终日志build/GUI_HostTest/h6_keyboard_bottom_final_build.log和h6_keyboard_bottom_final_test.log；早先build/test日志保留。未重跑无关触摸驱动或旧SPI。
- **ARM及边界**：原cmake --build --preset build-debug实际编gui_demo/gui_app/instrument_ui/GUI_Task共4源并链接退出0，build/Debug/h6_keyboard_bottom_build.log无编译warning/error/FAILED，RAM105184B/Flash754364B/CCM0，比上轮+8B/+824B。没有新增源文件/CMake选项、调试显示或运行计数，没有改驱动/坐标/时钟/接线/库核心，没有删除文件/下载或亲测。只新增范围待实机，原H6/H6.5实机验收状态保留。
- **下一步**：下载当前Debug复位，直接点底行五键下方空白，确认对应按键/光标且不串键；不用再采原始参数或重复中央/卡死测试。补充确认后主线从H7接上。

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

### 5.5 H7用户验收收口与A1.1矩形尺寸计算原指导（当前结论见5.7）

- **证据和边界**：用户明确“都已验收 功能正常 指导下一步”，H7现有四页显示、交互及模拟业务记实机验收通过；H4.2显示功能/H4.3界面功能同步收口。没有本轮独立下载/亲测或资源峰值数值。工作期间检测到其他工作同步新增键盘底行下延实现及进度，完整保留其历史与单项待实测状态，不覆盖源码，也不把本次验收外推到后来新增范围。
- **源码事实**：gui_demo.c的step仍以固定数值构造模拟结果，矩形primary_cmm=3200、secondary_cmm=2000。instrument_model.c已有四点平面标定和instrument_map_point，但没有矩形长宽计算函数；GUI_Task仍GUI_AppInit(NULL)，Flash后端未接。本轮只读检查.ioc/CMake/lv_conf及相关源码，未改业务代码、引脚、时钟或第三方库。
- **本轮唯一目标A1.1**：在User/GUI/instrument/instrument_model.h的InstrumentPoint声明之后、#endif之前追加函数声明，在同目录instrument_model.c追加实现，复用InstrumentPoint及math.h。接口为bool instrument_rectangle_dimensions(const InstrumentPoint points_mm[4], int32_t *length_cmm, int32_t *width_cmm)。输入是已知矩形四角，顺/逆时针沿边排列，单位毫米；该起步函数不识别任意四边形或拟合原始轮廓。计算四条欧氏边长，两组对边各取平均，较大值为长、较小值为宽，乘100四舍五入为现有结果使用的0.01mm整数。
- **错误与单位约束**：空指针、非有限坐标/边长、零边长、结果超过现有1000000单位上限或四舍五入后为0，返回false且不修改输出；成功同时写入两个输出并返回true。后续像素输入应先通过已有标定映射得到毫米坐标，避免把像素当毫米或重复套标定。
- **本小步验收及续接**：用户保存声明/实现后运行cmake --build --preset build-debug并提供日志，核验后只记代码/编译结果。本轮尚未替用户编写或构建函数。下一小步用理想32×20mm及旋转矩形输入检查应得到3200/2000，再接模拟后端/界面；不把编译或理想数据当实物尺寸/精度验证。Flash持久化、完整负载资源量化及真实测量仍未完成，后续主路线保持A1→C1→V1→V2→D1。

### 5.6 当前Debug资源盘点（2026-10-18，只读分析）

**范围与证据**：用户要求分析资源，提供构建末段和J-Link下载日志。实际用工程现有GNU Arm size/objdump/nm检查`build/Debug/03_F407_HAL_FreeRTOS.elf`及同名map；详细模块统计基于18:55:24的ELF/Map，ELF SHA256为`601e713bc6d786c5e4c455573b6f067536d9681e8a8870354cfb08406ce83b22`。分析期间检测到instrument_model.h/.c同步保存了矩形函数，并产生19:02:00的新ELF（SHA256 `52ab7871426cd0e355bdf9b41d7d6145808a18a192f607a91857459ff1cf2a11`，文件6445024B）；重新读取其size，所有已分配节大小均不变，nm未含该函数、map将其`.text`796B列在Discarded input sections中，未引用尚未进入固件。未覆盖同步改动、代编写或独立验收该函数。本资源轮未重新构建、下载或读取板上变量；用户日志证明当次loadfile比对一致并reset/go，不外推为后续新ELF已下载、键盘范围或算法实测通过。主线A1.1不在本轮升级。

| 存储区 | 已链接使用 | 容量 | 链接余量 |
|---|---:|---:|---:|
| 普通SRAM | 105184B（102.72KiB，80.25%） | 131072B | 25888B（25.28KiB） |
| CCM | 0B | 65536B | 65536B |
| 片内Flash | 754364B（736.68KiB，71.94%） | 1048576B | 294212B（287.32KiB） |

RAM来自`.data=664B + .bss=102984B + ._user_heap_stack=1536B`。后者是链接器对C库堆至少512B、主栈至少1024B的空间检查，不是FreeRTOS各任务栈。主要构成为：

| SRAM项目（互不重复） | 字节 | 解释 |
|---|---:|---|
| LVGL内置动态内存池 | 49152 | `work_mem_int.0`；控件、样式和临时绘图分配在池内消耗，不再增加链接总量 |
| FreeRTOS heap_4堆 | 24576 | `ucHeap`；四个动态应用任务的栈/控制块及动态信号量/软件定时器从这里分配 |
| LVGL局部绘图缓冲 | 19200 | `draw_buffer=480×20×2`，地址0x2000175C；不是整屏缓存 |
| GUI模型/界面/页面静态结构 | 4804 | model1920 + ui2376 + page508；ui已含1920B回滚模型，不能再次累加 |
| OLED显存 | 1024 | `OLED_DisplayBuf` |
| LCD DMA发送暂存 | 960 | `tx.0=0x2000678C`，普通SRAM；逐行转换RGB565字节序后发送 |
| Idle/Timer静态栈及控制块 | 1216 | 两个512B栈、两个96B控制块，在`.bss`内，不从ucHeap扣 |
| C堆/主栈最低预留 | 1536 | 512 + 1024 |
| 其他变量、锁、队列、库状态及对齐 | 2716 | 含`.data`与LVGL全局结构等 |
| 合计 | 105184 | 与链接输出一致 |

前三项合计92928B，占普通SRAM70.90%。FreeRTOS任务配置为GUI2048字/8192B/优先级2，Key128字/512B/优先级3，OLED256字/1024B/优先级1，Buzzer128字/512B/优先级1；上述四任务栈合计10240B，已经包含在24576B堆中。Idle和软件定时器服务任务各128字/512B，优先级分别0/4，为静态分配；共6任务。GUI按键静态队列8×4B数据+80B控制块；I²C互斥锁、SPI递归互斥锁及DMA完成信号量各80B静态控制块。main的OLED信号量、Buzzer信号量及LED软件定时器为动态对象；软件定时器命令队列长10、120B数据+80B控制块为静态。静态对象已在表中计入。

**三套分配边界**：LVGL48KiB池、FreeRTOS24KiB堆与newlib C库堆彼此独立。普通SRAM链接余25888B不会自动扩大前两者；任务栈是在任务创建时从RTOS堆预留，不是随栈水位增加而另占链接空间。当前`_end=0x200194E0`，`_sbrk()`按`_estack-_Min_Stack_Size=0x2001FC00`限制C堆，理论可增长26400B（含最低512B），不是只有512B可用；C库运行中分配可能继续消耗链接表中未静态分配的空间。未读当前运行堆/栈，不能把链接余量当实机空闲。2026-10-17的14152B RTOS堆余量、22140B LVGL余量、GUI栈水位770字仅是旧固件单次历史快照，不能代替本版峰值。

| 片内Flash模块（最终保留内容） | 字节 | KiB |
|---|---:|---:|
| LVGL核心及内置资源（含默认Montserrat14） | 533150 | 520.65 |
| 五份自定义GUI字体 | 107493 | 104.97 |
| GUI应用与显示/输入适配 | 43366 | 42.35 |
| STM32 HAL/CMSIS | 17573 | 17.16 |
| FreeRTOS | 13893 | 13.57 |
| C运行库/数学库 | 25098 | 24.51 |
| 用户硬件驱动 | 8497 | 8.30 |
| 用户任务代码/常量 | 1224 | 1.20 |
| 启动及Core | 3558 | 3.47 |
| 对齐及节间空隙 | 512 | 0.50 |
| 合计 | 754364 | 736.68 |

统计只取map最终链接部分，未把Discarded input sections或整个liblvgl.a文件大小算进芯片；C库合并字符串的map条目有重叠，按最终`.rodata`节实长归并70B，避免重复计数。Flash节分解为`.text=595764B`、`.rodata=157516B`、初始化数据镜像664B、向量表392B、ARM索引及初始化数组16B、节间空隙12B。ELF磁盘文件6444308B含调试信息及ELF布局，不代表烧入了6MiB。CXX链接方式未见ThorVG/C++运行时相关符号，不可仅因Linking CXX认定这些功能开启。

五份字体分别为12号26121B、14号34181B、16粗体43686B、24粗体1532B、28粗体1973B；均为只读Flash常量、不常驻RAM整份副本。LVGL自带Montserrat14另13649B，已包含于LVGL模块，全部上述字体合计121142B（118.30KiB）。本次没有发现需独立列出的大型图片资产，主要bitmap符号来自字体。

**外设/时序资源**：CPU/HCLK168MHz，APB1 42MHz/APB2 84MHz；SPI1 PA5/PA6/PA7，配置42MHz、LCD片选PE0/Flash片选PA4，DMA2 Stream3 Channel3 TX Normal，DMA/SPI IRQ优先级5；I²C2 PB10/PB11配置100kHz，OLED0x3C/触摸0x38共线，触摸PE1复位/PE2普通输入，无EXTI；USART1 PA9/PA10为115200，当前无UART DMA请求；TIM6提供HAL毫秒时基，SysTick为FreeRTOS1000Hz。显示DC/RST/BL为PB0/PB1/PB14，按键PE13/14/15、LED PB2/PB6、蜂鸣器PC8、SWD PA13/PA14、HSE PH0/PH1。当前只配置一条外设DMA流；其他流不等于任意外设都可使用，后续按芯片映射分配。链接报告中的片内Flash占用不含板载W25Q128，当前`GUI_AppInit(NULL)`未接持久化后端。

**判断与后续**：普通SRAM比Flash更需要关注，但80.25%主要是固定池/缓冲预留，不能据此认定即将运行溢出；当前可继续A1小规模计算，新增轮廓数组及UART DMA缓冲需先做预算，完整480×320 RGB565帧需307200B，不能放入普通SRAM。CCM64KiB当前未使用且不会自动分配；按[ST数据手册总线图](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)，DMA无法访问CCM，现有DMA tx须保留普通SRAM。若以后放纯CPU算法数组/任务栈到CCM，还需配套分区及启动初始化，当前启动汇编只处理普通data/bss，不能只加section属性就认为完成。Debug实际-O0/-g3，已有Release预设为-Os/-g0，后续可独立比较优化体积；LVGL仍保留多像素格式混合（含RGB888/ARGB8888/RGB565_SWAPPED）和默认主题等，裁剪需核实际依赖；本轮不改配置/字体/堆/栈、不承诺节省幅度。CPU利用率、真实任务栈水位、LVGL最大可分配块及两池峰值均无本版新证据，不以15ms输入目标或历史3/8ms局部刷新推定实际性能，也不自动恢复用户已取消测试。

详细机器统计保存在`build/Debug/resource_usage_analysis.json`（派生产物，可重新分析现有ELF/map获得）。本轮资源分析结束，下一次仍从A1.1用户函数的源码/构建核验续接，随后A1→C1→V1→V2→D1。

### 5.7 A1基础函数验收与矩形测量闭环指导（2026-10-18当前任务）

- **最新教学要求**：用户明确“拆分太细了 请直接一点 一步就要做很多！”。从本轮起每轮围绕一个完整可验收功能，集中给必要的局部代码、编译和实机操作，不再把头文件声明、编译、单组数值测试各分一轮；本条覆盖先前细分节奏。用户仍请求指导，不等于授权代改业务源码。
- **A1.1源码/构建验收通过**：实际读到instrument_model.h:51声明、instrument_model.c:148实现，与上轮示例的四边距离/对边均值/长宽排序/0.01mm输出及错误不改输出一致。用户日志显示原build-debug成功链接，RAM105184B/Flash754364B/CCM0；只收到末段，不宣称完整构建全程无警告。源码UTC11:01:55、对象11:01:58、ELF11:02:00；nm确认对象包含函数，最终ELF无其符号，map在Discarded input sections列出0x31c=796B段，符合尚未引用时的--gc-sections行为，故体积不变不代表函数没编译。
- **电脑数值检查通过**：仅在build/A1_RectangleVerify内新增带中文注释的检查程序，MinGW13.1以-std=c11/-Wall/-Wextra/-Werror直接编译用户真实instrument_model.c和measurement.c并链接数学库；编译/运行退出0（build.log、test.log）。4组32×20mm（平放/旋转/首边为短边/逆序）均输出3200/2000；6组零边长、超过10000mm、NaN和三个空指针均拒绝且保留111/222输出。另以真实instrument_calculate_calibration和instrument_map_point验证两种成像配置的旋转像素四角，均输出3200/2000。没有修改正式测试/业务源码、重跑旧GUI或ARM构建/下载；电脑结果不代替F407实机/精度验证。
- **当前一次性接入目标**：指导局部替换gui_demo.c的step中MeasurementResult初始化至r.residual_cmm之前的结果生成段。矩形模拟像素为(200,100)/(404.8,253.6)/(308.8,381.6)/(104,228)，沿边顺序，第二成像配置整体乘2；对应50mm参考模拟标定后为32×20mm旋转矩形。必须先逐点map到毫米，再调用用户尺寸函数，替换旧写死3200/2000并避免二次套旧尺寸映射；圆环原3012/1206及既有模拟映射保留。矩形映射/计算失败保持零尺寸，通过既有complete校验报告数据异常，不用旧尺寸伪造成功，失败不写入测量记录。
- **完整闭环验收**：用户保存、原build-debug编译及下载，在mm/矩形模式用50.00mm模拟参考完成标定，测量页显示32.00/20.00mm、记录页同值，再次测量仍可用；另一成像配置重新标定后同值。现有示意轮廓按长宽绘制，不声称已显示真实旋转轮廓。本轮仅给出替换代码，实际接入和ARM/实机验收待用户；保留模拟标识、原模拟残差与仅内存边界，未实现完整轮廓拟合、公差或真实相机/精度。

## 6. 其余长期阶段的目标边界（不得遗漏）

- **H6 FT6336U**：原H6/H6.5按用户确认实机验收通过、临时调试已移除。同步补充键盘底行五键下方到Y319，代码/ARM/真实LVGL回归通过待实测；原验收和触摸/总线方案保留。本对话H7现有主流程已另获用户确认，当前主线A1.1，新增范围仍按单项证据记录。
- **H7 四页面**：2026-10-18用户明确确认现有四页功能正常，记实机验收通过；GUI任务统一运行LVGL，其他任务通过队列/通知传数据。保留仅内存/模拟边界，Flash未实现，资源峰值无新增数据，后来同步新增键盘范围仍按单项证据记录。
- **A1 测量算法**：矩形基础函数已通过用户ARM编译核验与电脑数值检查，当前按用户新节奏一次推进模拟像素角点→标定毫米→尺寸→UI/记录闭环，待用户接入及实机反馈。完整目标仍为圆心/直径、内外径、矩形轮廓拟合、标注、公差上下限、PASS/FAIL、记录及提示，真实测量/精度不提前通过。
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

| **2026-10-18 / H6.5点击仍难响应，重启诊断显示** | 用户要求继续开启小蓝点和OLED数据；仅修改两个已有诊断宏及中文注释，保留Y镜像和原采样/总线配置。原build-debug实际编译链接退出0，RAM105328B/Flash754724B/CCM0，ELF含圆点/诊断函数；开启圆点的真实LVGL输入目标单独1/1通过。H6/H6.5转进行中，待新固件圆点及OLED数据，不标实机修复完成 |

| 版本/阶段 | 重要更新 |
|---|---|
| **2026-10-18 / H6.5触摸稳定性局部修复** | 用户报告圆点漏显/跳动、OLED N/P与坐标不对应及导航难点，授权修复。读帧前清零N/P缺陷已用旧驱动独立回归复现；改完成后发布和OLED快照，增加两槽编号跟踪/三点中值/15ms输入目标/导航外扩4像素。ARM RAM105448B/Flash755788B、驱动与真实LVGL两个目标各1/1通过，保留Y镜像及两个诊断开关；未下载，实机待复测 |

| **2026-10-18 / H6.5短按偏移与400/415定位** | 用户补充短点偏上/下而拖动准，同一点约400/415两档跳，400正确、415偏下；OLED字段/板上固件未核实。核实首帧直接通过与中值对持续交替无抑制，仅分析算法边界，未判屏坏；本轮只改进度，下一步确认数值字段并比较原始R/最终XY |

| **2026-10-18 / H6.5未报告点槽残留修复** | 用户确认原始R也跳；发现上轮固定解析两个槽未受N限制，可能采信未报告旧数据。对照Espressif官方驱动改按count限定，更正旧N1扫第二槽测试假设；修正前回归失败、修正后驱动1/1与ARM通过，RAM105448B/Flash755788B，未下载。下一步同点短按复测，不判屏坏 |

| **2026-10-18 / H6.5三图确认原始X/屏幕Y偏差** | 图片R=(16,433)/(3,433)/(51,431)、XY=(433,303)/(433,316)/(431,264)，用户首张准后两偏；N1/P1/S0/ER0，主要变化实际在原始X/屏幕Y，更新旧口述轴名。不能据图判屏坏，本轮仅更新进度，下一步中央同点短按5次比较R第一数范围 |

| **2026-10-18 / H6.5中央稳定及边缘算法检查** | 用户确认中央R第一数稳定，仅上下边缘偶发跳且多数圆点靠边框。只读检查未见额外吸边，电脑真实驱动加总线替身验证320个稳定输入、上下孤立跳点与首触3/16/16→Y316/316/303；首点限制有软件证据，不能代替实机根因。保留其他工作同步的总线/任务源码，本轮只新增build内探测及更新进度；下一步上/下内侧约20像素各按住1秒取稳定R/XY，H6.5进行中 |

| **2026-10-18 / H6.5测量完成触摸/OLED停更** | 用户确认KEY仍切液晶页、OLED连底部提示也停；应用真实指针连续3次完成后仍采样/导航1/1通过，未复现输入timer被停。修复共享I²C2无故障恢复的路径，持锁限次补时钟/STOP/重初始化，留存原错误并在SPI液晶显示；驱动/恢复/失败报告3/3与ARM通过，RAM105512B/Flash757660B，未下载实测。同步中央稳定/边缘算法证据保留，初始触发及两症状关联未确定 |

| **2026-10-18 / H6.5卡死恢复确认及边缘落点/跳动修复** | 用户确认不再卡死，中央稳定，要求用旧三图修边缘且不方便补数据。局部改边缘真实样本历史、GUI首次2/3样本确认/持续Y跳动确认/单样本短按保留，OLED XY改最终坐标；不裁边/猜空间系数。驱动/恢复/退出3/3与真实输入1/1通过，ARM RAM105600B/Flash758764B无编译warning/error；未下载实测。下一步只下载沿原操作反馈边缘与导航改善，不重复中央/参数采集，持续空间偏差未认定修复 |

| **2026-10-18 / H6.5商家对照及简化** | 实读本地商家F1/STC32_4SPI ctp/ctiic和LCD方向/初始化，改为1+4字节、仅CONTACT、硬复位默认模式；撤回中值/边缘确认/跨帧跟踪/补按/余辉，保留方向及共享总线恢复。旧商家契约失败、现驱动/恢复/退出3/3与GUI1/1及ARM通过，RAM105504B/Flash757140B，未下载实测；OLED加E事件，下一步下载直接验点击，不追加参数采集 |

| **2026-10-18 / H6.5商家原函数协议审计** | 简化版用户确认略灵敏但仍偏；F1/STC32原扫描函数与当前真实驱动各1819136组单指对照通过，核HAL地址/重复起始及事件/坐标位，未发现协议误读。未改固件算法或重建ARM，后续需实物读回/模组对照，不要求重刷同一版 |

| **2026-10-18 / H6.5 OLED停用+400kHz试验** | 按用户要求停OLED开机访问/任务，i2c.c与.ioc同步400kHz；解析/方向/蓝点不变。原ARM编译及ELF核验通过，RAM104360B/Flash749292B，待下载对比边缘和四导航，不把组合试验当单项根因确认 |

| **2026-10-18 / 撤销OLED停用及400kHz** | 用户要求只撤销上一轮；恢复OLED启动/任务与100kHz、移除试验开关及条件编译，商家触摸/蓝点/防卡死修复保留。原build-debug及ELF核验通过，RAM105504B/Flash757140B，未下载；不继续该试验 |

| **2026-10-18 / H6.5按钮触摸范围扩展** | 按用户请求，四导航外观/坐标保留、下方命中到Y319，其他独立按钮最多8像素并按邻键间隙中线分界；父容器与弹窗限制保留。原ARM实际编2源并链接通过，RAM105504B/Flash757852B；真实LVGL指针单独1/1通过，覆盖底边全像素/按住偏移、外扩点击、密集行边界、禁用及遮挡，未下载。原定位偏差未认定修复，H6.5继续待实机，不改驱动/映射/总线或恢复滤波 |

| **2026-10-18 / H6.5用户验收完成、清理调试** | 用户明确确认H6.5完成；H6及H6.2～H6.5按功能验收收口，下一阶段H7。删除临时液晶I²C条/OLED触摸数据/光标及输入、采样、刷新、栈堆、SPI统计；保留按钮扩展与互斥恢复/DMA/错误保护。ARM RAM105176B/Flash753540B，无编译warning/error，GUI1/1及驱动/恢复/退出3/3通过、ELF符号核验通过。清理版未下载/亲测，全部历史与长期阶段保留 |

| **2026-10-18 / H6.5键盘底行补充** | 按用户新请求扩数字键盘底行+/−、0、.、左、右下方到Y319；键盘/单键外观和上三排不变，原键盘处理输入，保留禁用/长按及滑动取消。原ARM实际编4源链接通过，RAM105184B/Flash754364B；真实LVGL输入单独1/1通过，未下载。原H6.5验收保持，仅新增范围待实测，随后接H7；无调试显示/统计仍保留清理状态 |

| **2026-10-18 / H7用户验收、转A1.1指导** | 用户明确“都已验收 功能正常 指导下一步”，H7现有四页显示/交互/模拟流程和合并的H4.2/H4.3功能验收收口。只读核查GUI任务、模拟后端、模型及配置，Flash仍未接、矩形仍写死3200/2000；指导用户在既有instrument_model.h/.c添加矩形四角毫米坐标到长宽的纯函数并编译。只更新本进度，未代改业务源码或新增构建/下载；同步键盘代码和单项待实测记录保留，全部长期阶段保留 |

| **2026-10-18 / A1函数验收、按完整功能指导矩形闭环** | 用户提供ARM链接成功日志，源码/对象/ELF对应且新函数未引用被裁掉。直接编译真实模型源，4组矩形、6组非法输入及两种标定像素夹具通过（build/A1_RectangleVerify）。用户要求减少细分，改为一次完成矩形像素角点→标定毫米→长宽→页面/记录的局部接入、编译下载与验收；本轮只指导，未代改业务源码或ARM下载，全部旧状态/资源盘点保留 |

## 8. 下次从这里开始（避免重复问、重复做）

**下一次从A1矩形测量闭环的用户代码/实机反馈接上。** 基础尺寸函数已经源码/ARM编译核验和电脑数值验证，不再要求用户重复声明/单独基础数字测试。用户最新要求每轮一个完整功能，将相关修改、编译和验收一起安排；当前指导只改gui_demo结果生成段，见5.7。H6/H6.5及H7主流程验收、同步键盘补充单项记录和全部未提交改动保留。

**当前证据**：用户build-debug链接RAM105184B/Flash754364B/CCM0；源码/对象/ELF时间对应，函数已编译但因未调用被最终ELF裁掉。build/A1_RectangleVerify/build.log、test.log记录直接编译真实模型源的4组矩形/6组非法输入/两种标定像素夹具全部通过。详细资源盘点保留在5.6；本轮未改业务源码或重编ARM/下载，不把电脑检查或历史J-Link成功当新矩形闭环实机证据。

**下一步操作及验收**：用户替换gui_demo.c的step结果生成段，先将旋转像素四角经已有标定映射到毫米，再调尺寸函数；旧圆环路径保留，矩形不重复标定。保存、原build-debug编译并下载，用50.00mm参考/mm矩形模式验页面32.00/20.00mm、记录同值及再次测量正常，两种成像配置各重新标定后同值；一次反馈完整流程。尚未修改/编译接入段或取得其实机结果。后续仍A1→C1→V1→V2→D1，完整轮廓拟合、公差、Flash及真实精度不提前完成。

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
| 2026-10-18 | H6.5点击仍难响应，按要求重新开启小蓝点/OLED | 用户反馈当前触控非常不灵敏、点击仍无反应，授权重新开启诊断。仅将FT6336U_OLED_DIAGNOSTICS和GUI_TOUCH_TRACE设1并更新中文注释，保留交换后Y镜像、端点/周期/总线配置。原build-debug实际编译4个相关源文件并链接退出0，build/Debug/h6_touch_debug_reenable_build.log无编译warning/error/FAILED，RAM105328B/Flash754724B/CCM0，ELF含touch_cursor/show_touch_diagnostics；现有gui_touch单独1/1通过，build/GUI_HostTest/h6_touch_debug_reenable_test.log，未新增测试/重跑驱动旧SPI套件/下载/删除文件 | H6/H6.5转进行中，诊断配置/编译通过但剩余难响应原因未定，方向修正不当成整体实机通过；全部主阶段保留 | 下载本版复位，在底部第二个入口按住约2秒后松手，提供圆点对准/跟手/消失及OLED R/XY/N/P/S/ER/OUT和LP/LR/LC变化；依据新证据排障，稳定交互通过后收口H6接H7，不恢复H5取消测试 |
| 2026-10-18 | H6.5圆点跳动/漏点与导航难点局部修复 | 用户反馈新症状并授权修复；完整读规则/进度，先查git状态保留原三处未提交改动，核实.ioc/CMake/lv_conf及实际驱动/共享锁/OLED/GUI/LVGL/导航/测试。修正读帧前清N/P及OLED逐字段读实时变量：完成后短临界区一起发布、OLED静态快照。读完整两个点槽并按编号跟踪主手指，三点中值抑制孤立跳点，15ms输入目标及导航外扩4像素；Y镜像/端点/时钟/接线/两个诊断宏保留。旧版N2用例及独立发布时序用例实际失败、新版驱动1/1和独立快照回归通过；真实LVGL输入单独1/1通过含20ms短按/四导航边缘。原build-debug实际编译链接退出0，无编译warning/error/FAILED，RAM105448B/Flash755788B/CCM0，日志h6_touch_stability_build.log；OLED诊断静态栈24B，完整栈余量未实测。同步README和本进度，未删除文件/下载/实测或改第三方核心/引脚/时钟，未重跑旧SPI套件 | H6/H6.5保持进行中；本轮代码/编译/针对性主机验证通过，诊断中间状态缺陷有回归证据，实际跳点漏点全部根因仍未查实，不宣称实机修复完成；全部长期阶段保留 | 下载当前Debug ELF复位，单指按住拖动、松手及逐个点击四页，确认稳定跟手/状态一致/容易命中；仍异常给按住时完整OLED和ER/OUT、LP/LR/LC变化。H6通过后从H7续接，不恢复H5取消测试 |
| 2026-10-18 | H6.5短按偏移/重复400与415反馈定位 | 用户反馈短点圆点偏上或偏下、长按拖动准，继续补充同一处约400/415两档变化、400正确而415偏下；具体OLED字段和板上固件版本未确认。只读核实本地映射、首帧填满中值历史、LVGL光标处理及配置，纯数值演算确认交替输入不能被当前三点中值消除；这不是实机报文复现，不认定硬件损坏或根因已查清。已请求选择R/XY的哪一个数字，本轮只实际更新进度，未改业务代码/构建/下载/接线或新增测试 | H6/H6.5保持进行中，稳定点击未验；上一轮源码与构建证据保留，新增首触及持续交替的算法边界记录，全部长期阶段保留 | 确认400/415对应字段及所跑固件，再看同点按住不拖动能否自行回准、原始R与最终XY是否同步跳；据结果局部处理，H6实机通过后接H7 |
| 2026-10-18 | H6.5确认400/415是屏幕X | 用户通过选项明确400/415是OLED第二行XY第一个数。当前映射未滤波屏幕X=raw_y，三点中值只取历史值，Y镜像不改变屏幕X；尚未取得第一行R第二个数的同步对照，不能认定原始数据抖动或屏坏。本轮仅继续更新文档，未改源码/重编译/下载 | H6/H6.5保持进行中，数值字段已确认，原始输入层与滤波历史仍待区分 | 只看P=1时第一行R逗号后的raw_y是否也在400/415附近跳，并与XY首数比较；必要时再核初触不拖动能否回准，H6实机通过后接H7 |
| 2026-10-18 | H6.5原始R也跳：未报告点槽残留修复 | 用户确认R也会在“00左右和二三十左右”跳，完整范围未核实且不自行补百位。对照Espressif官方FT6336U驱动发现上轮固定遍历两槽未受N限制，可能追到未报告的旧ID/CONTACT或产生近零假按下。更正旧N1应扫第二槽的错误测试假设，新增N1旧主手指残留及首槽UP/第二槽近零CONTACT回归；修正前实际失败line153（h6_touch_report_count_before_test.log），FT循环上限改count后完整驱动1/1通过（h6_touch_report_count_test.log）。原build-debug实际编译FT/GUI_Task/OLED_Task并链接退出0，无编译warning/error/FAILED，RAM105448B/Flash755788B/CCM0（h6_touch_report_count_build.log）；同步中文注释/README/本进度，未改方向端点/时钟/引脚/库核心，保留诊断/中值/15ms/导航外扩。本轮未重跑GUI旧SPI、下载或删除文件 | H6/H6.5仍进行中；确定的软件点槽上限缺陷已修，实际跳点全部根因未查实，不判屏坏或标实机完成；全部长期阶段保留 | 下载当前Debug ELF复位，同一点重复短按对照R/XY与圆点；持续异常给P=1时完整OLED/短视频及S/ER/OUT和完整数值，稳定交互验收后接H7，不恢复H5取消测试 |
| 2026-10-18 | H6.5旁支：电脑测试弹窗修复 | 用户在侧边对话提供touch_driver_test.exe应用程序错误截图并明确要求检查修复。旧日志记录assert失败后异常退出；本轮只将主机测试检查改为始终执行的CHECK：输出文件/行号/条件，正常exit(1)，保留失败判定，不调用assert/abort触发Windows报告。新增独立失败报告自检，要求5秒内退出1且stderr包含位置和条件，不能将崩溃或超时算通过。隔离目录build/H6_TestExitVerify先2/2通过，再更新原build/H6_TouchTest测试程序并2/2通过（正常回归0.05秒，失败报告自检0.03秒，总0.09秒），日志test_failure_exit_fix_build.log、test_failure_exit_fix_test.log。未改STM32驱动/固件/系统错误报告设置，未续做主线触摸修复，仅追加本日志避免覆盖主线进度 | 本旁支测试工具修复完成；主线H6/H6.5状态及实机验收保持原状，主机通过不证明实物触摸稳定 | 以后测试失败直接查看CTest失败位置和条件；旧错误窗口若仍保留可点确定关闭；主线继续由原对话处理实机触摸问题 |
| 2026-10-18 | H6.5三张OLED照片证据与排查轴更正 | 用户提供三张图并标首张准、后两偏；以view_image实际查看原尺寸：R=(16,433)/(3,433)/(51,431)，XY=(433,303)/(433,316)/(431,264)，三图N1/P1/S0/ER0；LP/LR/LC依次78/77/74、82/82/79、88/87/84。主要变化在原始X与屏幕Y，屏幕X431～433基本稳定，取代先前按口述聚焦屏幕X/raw_y的方向；第三图Y264与当帧319-51=268差4与中值历史相容，未据此判映射错误。图未显示触摸面板/手指或波形，不能区分面板本体、电气环境和读回软件，也不能由S0/ER0判所有数据可靠。只读核查本地映射/诊断及ST官方电容触摸噪声通用资料，仅实际更新本进度，未改代码/测试/构建/下载/接线；保留旁支日志和原修复证据 | H6/H6.5进行中，新增可核实的原始X/屏幕Y偏差证据，稳定点击未验，不认定屏坏；全部主阶段保留 | 屏幕中央同一点垂直短按5次，不拖动、不改固件接线电源，比较R第一数范围与下边缘样本；按结果再区分边缘/落指因素与全屏采样/电气环境或读回，H6实机通过后接H7 |
| 2026-10-18 | H6.5中央稳定、上下边缘算法检查 | 用户完成中央对照，R第一数稳定不差几十，顶部/底部才偶发上下跳且多数圆点靠边框。只读核map_point、median、松手清历史、光标居中及.ioc/CMake/lv_conf，未发现额外吸边或整数回绕。build/H6_TouchTest内新增探测，真实FT驱动配HAL/RTOS/总线替身，320个raw_x各4帧稳定、上下孤立跳点通过；指定初触3/16/16输出Y316/316/303，首点立即通过的限制被验证，不能纠正持续空间偏差。首次联编总线时遇其他工作新增GPIO/恢复接口与旧替身不匹配，改用总线替身隔离后-Wall/-Wextra/-Werror编译/运行退出0，日志h6_edge_algorithm_probe_build.log/probe.log；不把此称完整旧回归或总线恢复验收。业务源码及正式测试未由本轮修改，同步改动保留，无ARM构建/下载/实测；文档顶部/状态表/当前节/续接同步保存 | H6/H6.5继续进行中，中央对照按用户确认收口；边缘持续偏差与初触现象待区分，未判屏坏或擅调标定；全部主阶段/历史证据保留 | 不重复中央测试；同一竖线距上/下边框约20像素内侧各按住1秒，提供稳定R第一数/XY第二数及是否回准。理想上299/20、下20/299只作粗对照；稳定偏差再核标定，初触偏移再评估首点处理，H6通过后接H7 |
| 2026-10-18 | H6.5测量完成后触摸/OLED停更：共享总线恢复及采证 | 用户确认故障在显示测量完成时，实体KEY仍切液晶四页，OLED底部提示/OK也停。实际检查完成回调、定时器、共享锁/HAL超时；真实指针标定后连续3次完成仍采样导航及内存检查1/1通过（h6_measure_complete_touch_test.log），未复现应用停输入。修复I2C2_Bus只有互斥无硬件故障恢复的缺口：HAL返回且原错误保存后持锁上报，忙/超时等每秒最多一次关闭/复位、开漏释放PB10/11、必要时9个数据时钟/STOP、归还AF4/原100kHz；卡低/Init失败有界退出，锁失败/NACK不重置，失败帧仍松手。g_i2c2_bus留存地址/错误/寄存器/电平/恢复计数，SPI液晶故障后显示E/K/R/A/S/L；OLED栈128至256字，运行时堆多用512B。真实共享层故障模型、触摸回归、原失败报告3/3通过（h6_i2c_recovery_build/test.log），ARM实际编译链接两次退出0，最终RAM105512B/Flash757660B/CCM0无编译warning/error（h6_i2c_recovery_final_build.log）。未下载/测波形/实机恢复/删除文件，中央稳定及边缘算法同步工作完整保留，不重复中央测试 | H6/H6.5进行中，代码/编译/主机通过，实机初始触发和恢复效果待验；不认定与边缘跳点同因或屏坏，全部长期阶段保留 | 下载当前Debug复位/重新模拟标定并测量3次，完成后触摸切页且OLED持续更新；异常拍SPI液晶I2C条与OLED、没条也反馈。该问题稳定后接已安排的上下内侧稳定点对照，再收口H6接H7，不恢复H5取消测试 |
| 2026-10-18 | H6.5边缘现象澄清及进度复核 | 用户明确蓝点比手指更靠上/下边框且上下波动，并指向process说明。定位工程内PROJECT_PROGRESS.md，核对原三图R第一数16/3/51、中央稳定、拖动较准、首点直接通过及持续两档变化的中值边界；再次只读核当前FT映射/滤波未变。更新先前“靠框含义待确认”为已确认向框偏并伴波动；不能只归于首点，也不能据指定输入测试认定实物标定正确。本次仅修改本进度，无源码/编译/测试/下载 | H6/H6.5保持进行中；保留最新共享I²C2恢复代码/构建及待实机状态，不回退或覆盖其他工作；全部主阶段保留 | 不重复询问靠框含义或中央测试，不新增本轮操作；边缘后续分别核空间对应与初触/连续跳点，沿用现有停更恢复验收优先级，H6实机稳定后接H7 |
| 2026-10-18 | H6.5不再卡死用户确认，边缘首次落点/跳点修复 | 用户确认测量结束不再卡死、中央R稳定，仅上下边缘向框偏和偶发上下波动，明确要求修复；后续说明不方便新采数据，要求使用已提供三图，不再索要同组参数或中央测试。旧图准确Y303、偏316/264不能支持统一平移。FT边缘首次改真实样本历史，中央原策略保留；GUI对上下64像素首次等接近的两点或三点中值，持续边缘Y跳变>8先确认，真实新位置/同向快速拖动仍跟随。真实单样本短按松手后在同轮补交已知按下/释放，保留点击、不手发CLICKED/多读I2C/延长按钮按住，错误取消；短按圆点留80ms视觉提示。OLED XY改最终g_gui_touch位置，R/N/P仍驱动层，新增pending/等待/确认/短按计数。驱动新增3/16/16和320原始X全域稳定回归，与总线恢复/失败退出3/3通过（h6_edge_settle_test.log）；真实LVGL照片构造流/交替跳/顶部/0和319/中央/快速拖动/错误取消/单样本和20ms短按/四导航/3次测量完成后交互1/1通过（h6_edge_settle_final_test.log）。原ARM实际编4源并链接退出0，无编译warning/error，RAM105600B/Flash758764B/CCM0（h6_edge_settle_build.log）。未改方向/端点/频率/引脚/第三方库/已恢复的总线，未删除或下载实测；全部原改动与旁支历史保留 | 卡死恢复效果按用户反馈收口；H6/H6.5进行中，边缘代码/编译/软件回归通过待实机。时间确认不等于空间标定，不能保证消除稳定报偏或单样本位置错误，不判屏坏 | 下载当前Debug复位，直接沿原方式点顶部/底部/四导航并观察拖动松手，反馈蓝点/按钮是否改善；不再抄新参数、补同组三图或重做中央测试。实机稳定后接H7，不恢复H5取消测试 |
| 2026-10-18 | H6.5底部按钮原始X持续0，偏移仍在 | 用户新报R=0000,0177仍偏，进一步确认手指在底部按钮位置、按住后读数不变。只读核R取原始字段、有效N/事件/ID点槽解析在映射前赋值；基础映射(0,177)→(177,319)，没有越界夹零分支，导航主体约Y284～308/中心296。若为按住时实时有效读数，则异常已在原始读回/触摸端，时间滤波不能恢复持续0背后的实际位置；本次N/P/OK及ELF对应未独立取得，仍需保留旧点/通信异常边界，不直接判物理屏坏或要求换屏。本轮仅更新进度及源码核对，无代码/测试/新构建/下载/删除；既有ARM与主机通过保留，不当成实机偏移解决 | H6/H6.5进行中，稳定边缘偏移仍在；不再卡死反馈继续收口，不认定两症状同因，全部长期主阶段保留 | 优先区分实时有效原始读回、共享通信与触摸模组边缘定位，不继续盲堆滤波/固定平移/把0硬改为按钮位置；不重复索要用户不方便提供的同组参数或中央测试，稳定交互后接H7 |
| 2026-10-18 | H6.5读取商家源码并修正简化触摸 | 用户要求停止讨论、读取D:/.Data/spi 3.5寸屏幕资料/CL35BC1070-40A_CTP直接修复，明确此前处理过多。实际以GBK读F1_4SPI ctp.c/.h、ctiic.c，核STC32_4SPI以及LCD方向/初始化/独立代码txt；商家只读，未运行其脚本。差异为硬复位后不写模式、先0x02读1字节再0x03读4字节、仅高半字节0x80 CONTACT有效、双零忽略。已据此修FT读取/事件/默认模式，撤回中值/边缘等待/跨帧编号/短按补交/圆点余辉，GUI直接转交；本工程单指，多指松手，原始单轴0仍合法，额外只读身份供诊断，同一I2C锁覆盖两笔HAL，保留F407方向/时钟/已恢复总线。OLED加E事件以辨DOWN/CONTACT；LCD0x28和INVON与商家横屏dir3一致，未混抄两份商家各不同的电源/gamma。旧商家契约实际失败于模式写入（h6_vendor_before_test.log退出1）；现驱动/恢复/失败退出3/3和独立商家契约退出0、GUI直接输入/20ms有效点/四导航/三次测量后交互1/1（h6_vendor系列日志）。原ARM实际编4源、最终OLED再编链接退出0，无编译warning/error，RAM105504B/Flash757140B/CCM0（h6_vendor_touch_final_build.log）。更新正式契约测试以对应撤回功能，保留CHECK工具修复及全部原历史，未删除/下载/亲测 | H6/H6.5进行中，商家对照修正及简化代码/编译/主机通过待实测，不再提前归咎手指或硬件，也不宣称全部偏移已修 | 下载当前Debug并复位，直接点顶部/底部/四导航、拖动松手反馈；E2才有效CONTACT，R/XY沿当前状态读，不重复两组参数/中央或已收口的卡死测试，稳定后接H7，不恢复H5取消测试 |

| 2026-10-18 | H6.5商家原函数协议审计 | 用户实测简化版略灵敏但仍偏。读F1/STC32扫描/软件I²C/LCD方向/测试调用和厂家寄存器说明；原样编译两份ctp.c与当前真实FT驱动，电脑替身各1819136组单指对照通过，HAL地址方向/重复起始、事件/编号屏蔽/高低字节无已发现差异。日志build/H6_TouchTest/vendor_audit/{build,test,stc_build,stc_test}.log；F407方向、单指和RTOS必要适配明确列出。仅更新README/进度与build内审计产物，未改固件源码/ARM/下载/删除 | H6/H6.5仍进行中，响应略改善有用户证据，偏移未解决；不把软件通过当屏体正常或损坏 | 后续从实物读回/模组对照定位，不加滤波/猜端点，不要求重刷相同固件或重复中央/同组参数采集；通过H6后接H7 |

| 2026-10-18 | H6.5 OLED停用+I²C2 400kHz隔离试验 | 用户明确授权。OLED_TASK_ENABLED=0使main不探测/初始化/刷新OLED、不创建其信号量与任务；i2c.c ClockSpeed及.ioc同步400000，恢复沿用当前Init，触摸解析/方向/蓝点不变。原build-debug实际编译链接退出0、无编译warning/error，RAM104360B/Flash749292B/CCM0，h6_oled_off_400k_build.log；ELF无OLED函数，保留FT/GUI/蓝点，反汇编确认400000。未加算法/新测试、未重跑旧主机回归或下载/测波形/删除文件 | H6整体进行中；本隔离版配置/编译通过待实测，原偏移仍未收口 | 下载当前Debug复位，直接对比顶部/底部/四导航与拖动松手；OLED旧画面可能残留，组合效果不等同单项根因，禁止把编译通过写成实机修好；通过H6后接H7 |

| 2026-10-18 | H6.5撤销OLED停用/400kHz试验 | 用户明确撤销上一轮。恢复OLED开机探测/初始化/刷新任务和信号量，移除OLED_TASK_ENABLED及main条件编译；i2c.c与.ioc回到100000，触摸/蓝点/防卡死修复保留。实际ARM编4源及链接退出0、无编译warning/error，RAM105504B/Flash757140B，ELF恢复OLED函数且ClockSpeed=100000，日志h6_restore_oled_100k_build.log及symbols/i2c_disassembly.log；未下载/实测/删除文件 | 此次撤销完成；H6/H6.5原偏移仍进行中，无试验实物效果结论 | 等待用户进一步指示；若板上曾下载试验版才需重刷当前Debug恢复，不追加测试或新改动 |

| 2026-10-18 | H6.5商家回复含义评估 | 用户提供两张聊天截图，只要求解释。商家称通用触摸内部固件可对批量产品协调定制，毕设可由主控排除偏离点；区分FT内部程序与STM32驱动，说明“未适配UI”笼统且不是根因证明。原手册确认内部MCU/Flash及主控接口层次；旧偶发偏点可考虑筛选，持续原始0对应Y319不能靠丢点还原，合法边缘不能一律判坏。完整读进度/规则、git状态及相关源码配置，只实际更新本进度，无源码/测试/构建/下载/新实机证据 | H6/H6.5保持进行中；OLED及100kHz回退结果、原解析/方向/蓝点/防卡死和全部长期阶段保留，不标偏移已解决 | 后续若实施先明确异常点判据和持续偏差边界；本轮不追加操作，不重刷同版或重复中央/已拒绝采数，不因商家建议恢复历史多层算法；H6实机通过后再接H7 |
| 2026-10-18 | H6.5按钮触摸范围扩展 | 用户明确要求四导航覆盖原位置加下方空白，其他按钮适当扩大。局部改measurement_page/instrument_ui：导航归整屏root而外观/坐标不变，左右/上4像素、下到Y319；其他独立按钮最多8像素，按当前可见可点击兄弟按钮的空隙中线限界，密集2像素行距各用1像素，隐藏/禁用/弹窗逻辑保留。LVGL9.6命中结构需其event_private头，首次缺声明/调用未公开函数编译失败后已修正，原日志保留，未改第三方核心。原ARM实际编2源并链接退出0，无编译warning/error/FAILED，RAM105504B/Flash757852B/CCM0（h6_hit_area_final_build.log）。真实LVGL输入目标单独1/1通过，含下方全像素/底边按住移动、外扩测量/记录/设置/返回/取消、密集行边界、禁用及确认/编辑层遮挡和原交互回归（h6_hit_area_extended_build/test.log）；未重跑无关旧SPI/驱动套件。更新README及本进度全部当前入口，保留原未提交改动，未改触摸驱动/映射/时钟/接线/OLED/总线，未恢复滤波、删除文件或下载亲测 | 本小步配置/编译通过待实测；H6/H6.5仍进行中，命中容错不等于定位偏差根因修复，全部长期阶段保留 | 下载当前Debug复位，直接点四键下方空白和页面按钮边缘，反馈是否容易命中且无串键；不抄新参数、不重复中央/同组采数及已收口卡死测试。稳定实机交互验收后接H7 |

| 2026-10-18 | H6.5用户确认实机验收、清理调试并交接H7 | 用户明确“删除所有的调试功能 包括屏幕上的I2C，OLED的数据 还有小蓝点，H6.5验收完成”。按当前交互方案记录H6/H6.2～H6.5实机验收通过；局部移除液晶I²C故障条/OLED触摸快照和字段/蓝点/事件与采样统计/刷新耗时/栈堆监测/SPI计数、公开诊断类型和宏。FT仅核芯片ID，保留最后坐标/就绪/重试；保留方向/CONTACT/15ms、按钮命中扩展、OLED原内容/按键提示、I²C100kHz与互斥限次恢复、DMA等待/回调通知/安全收尾及错误保护。真实LVGL输入单独1/1和触摸/共享总线恢复/失败报告3/3通过，观测器仅主机测试使用；原ARM实际编9源并链接退出0、无编译warning/error/FAILED，RAM105176B/Flash753540B/CCM0，日志h6_debug_cleanup_build/test/symbols系列。ELF无临时显示/统计符号，保留功能链路。更新README及本进度全部当前入口，未删除文件/下载/亲测或改时钟引脚/第三方库，全部原未提交改动和历史保留 | H6/H6.5实机验收通过依据用户确认；本轮清理版代码/编译/相关回归通过但未下载，H7仍配置/编译通过待整体验收，远期阶段不升级 | 下载当前Debug复位使清理生效；不重复H6.5/中央采数/卡死测试，下一次从H7四页面整体验收接上，按用户后续指示推进 |

| 2026-10-18 | H6.5补充：数字键盘底行五键下延 | 用户要求+/−、0、.、左、右五键像四导航一样把下方延伸到屏幕底边。局部改instrument_ui.c/.h：矩阵外框向下7像素，HIT_TEST及实际布局中线分列，新增区选中原键并交原VALUE_CHANGED处理；私有按住键号保持长按、同列偏移/异列取消和禁用限制。原466×192位置(7,121)、17键布局/上三排/输入规则/坐标/驱动/硬件/调试清理保留，不改LVGL核心。原ARM实际编4源并链接退出0，无编译warning/error/FAILED，RAM105184B/Flash754364B/CCM0（h6_keyboard_bottom_build.log）；真实GUI输入单独1/1通过，覆盖五键可见底边到Y319所有下方行/文字光标/相邻分界/上三排/负边界/禁用/长按/拖动取消/关闭及原交互（h6_keyboard_bottom_final_build/test.log）。更新README与本进度全部当前入口，保留用户已有改动/全部历史，未删除文件、下载/亲测或重跑无关旧SPI/驱动 | 原H6/H6.5实机通过保持；此次键盘补充配置/编译通过待实测，H7/长期阶段不提前升级 | 下载当前Debug复位，只点底行五键下方空白到边框，确认对应键/光标及无串键，不重复旧参数/中央/卡死任务；确认后从H7继续 |

| 2026-10-18 | H7用户验收收口 / A1.1矩形尺寸函数指导 | 用户明确“都已验收 功能正常 指导下一步”。实际只读核查gui_demo固定尺寸、已有InstrumentPoint/平面标定、GUI_AppInit(NULL)、CMake/lv_conf/.ioc及git未提交改动；H7现有四页显示/交互/模拟流程和合并的H4.2/H4.3功能按用户确认收口。指导在既有instrument_model.h/.c添加四个毫米角点到长宽的纯函数：欧氏边长、对边均值、长宽排序、0.01mm整数和错误不改输出。只实际更新本进度，未代改源码、新建测试、构建或下载；过程中检测并保留其他工作同步追加的键盘底行代码及其独立待实测证据，未把本次验收外推到新增范围 | H7/H4.2/H4.3现有功能实机验收通过；A1进入进行中、当前A1.1待用户编写/编译。Flash/真实后端/资源峰值及全部长期阶段边界保留 | 用户保存函数声明和实现，运行原build-debug提供日志；代码/编译验收后再用32×20mm及旋转输入验证3200/2000并接模拟后端/界面，后续A1→C1→V1→V2→D1，不重开原H6.5/H7主流程测试 |

| 2026-10-18 | A1.1旁支：当前Debug资源盘点 | 用户提供RAM105184B/Flash754364B/CCM0及J-Link内容一致/reset/go日志；只读检查规则/完整进度/git状态、.ioc/CMake/lv_conf/任务/链接脚本和现有ELF/map，以size/objdump/nm及map保留节归类，已核普通SRAM链接余25888B、Flash余294212B。RAM主项LVGL池49152B、RTOS堆24576B、绘图19200B；四动态任务栈10240B已含RTOS堆，Idle/Timer静态栈共1024B，当前6任务。Flash LVGL533150B、五自定义字体107493B、GUI43366B，其余库/驱动/任务/对齐合计70355B；C库合并字符串按最终节修正70B防重复。详细分类/三套堆/外设/边界写5.6及build/Debug/resource_usage_analysis.json，未改工程源码、构建、下载或取得新实机峰值。分析期间检测instrument_model.h/.c新函数同步保存并生成19:02 ELF，size分配节与18:55基线一致；map将未引用函数796B列Discarded，nm未含最终符号，保留同步源码且不在本资源轮独立验收。首个旧SHA一致性检查因并行新ELF失败，随后核实新SHA与分配节未变，未误写为旧文件未变 | 本轮资源盘点完成；H6/H7原验收保持，A1.1继续进行中并记录同步源码存在/待本步核验，不将编译/下载当算法或新键盘范围实测 | 按A1.1核对新函数源码及构建证据，之后验证32×20mm和旋转矩形并接入调用；再看实际链接增量。运行峰值/CPU无本版新证据，不恢复已取消测试；后续A1→C1→V1→V2→D1 |

| 2026-10-18 | A1.1函数验收 / A1矩形闭环集中指导 | 用户原build-debug日志成功链接RAM105184B/Flash754364B/CCM0；实际声明/实现与指导一致，源码UTC11:01:55、对象11:01:58、ELF11:02:00，nm对象含函数但ELF无，map丢弃0x31c=796B未引用段。仅build/A1_RectangleVerify新增主机检查，以MinGW13.1 -Wall/-Wextra/-Werror编译真实model/measurement，4组矩形、6组无效输入不改输出、两种成像标定后的旋转像素点均通过（build.log/test.log）。用户明确“拆分太细了 请直接一点 一步就要做很多！”，教学改为每轮完整功能闭环，集中指导gui_demo局部替换、原预设编译、下载及页面/记录验收；未代改工程源码/正式测试、重编ARM、下载/亲测或删除文件 | A1.1基础函数代码/编译与软件数值验收通过；A1整体进行中，当前矩形像素→标定→尺寸→UI/记录接入待用户。H7/H6结论、同步键盘单项、资源盘点及所有远期阶段保留 | 用户按替换段保存gui_demo，build-debug编译下载，用50.00mm参考/mm矩形模式确认32.00/20.00mm、记录同值及再次测量；两种成像配置分别重标定后同值，一次反馈完整闭环，后续按完整功能推进，不恢复旧细碎拆分或触摸诊断 |
