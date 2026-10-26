# 智能视觉测量仪 LVGL 应用层

2026-10-25：用户已确认35001并口屏“已验收功能正常”。当前显示固定采用FSMC 16位并口，
源文件为`User/Hardware/ST7796_FSMC.c`，公共接口为`ST7796.h`。

## 构建与运行

```powershell
# 使用现有Arm GCC/Ninja工具链配置和编译正式固件。
cmake --preset debug-armgcc
cmake --build --preset build-debug
```

根CMake自动收集`User`目录源码，不单独声明显示文件、不保留总线切换或自检选项。
VS Code使用`Debug (Cortex-Debug J-Link)`入口，固件位于`build/Debug/03_F407_HAL_FreeRTOS.elf`。

## 显示与任务接口

- FSMC NE1=PD7、A16/RS=PD11、WR=PD5、RD=PD4；复位PB1、背光PB14。
- 480×320横屏，RGB565原生16位像素，命令0x60000000、数据0x60020000。
- `ST7796_WriteRGB565Start/Wait`异步刷新：DMA发送A时CPU绘制B，完成/安全停流后才`lv_display_flush_ready`。
- DMA2 Stream0/Channel0使用内存到内存、半字宽度、FIFO、普通模式；PAR源递增，M0AR固定为0x60020000。
- 紧密RGB565块整块发送，有stride填充才逐行续传；不换序、不复制像素、不把padding发送到LCD。
- 双480×32局部缓冲共61440B普通SRAM，LVGL软件池52KiB CCM；没有整屏帧缓冲。
- GUI任务独占LVGL/显示；其他任务通过队列/状态快照交互，触摸FT6336U使用原I²C2。
- OLED现有FPS/MS/BLK/ERR保留；块数不等于帧数，静止画面不要求持续刷新。
- 初始化保持MADCTL=0x28、INVON及当前5/15/5写时序；DMA减少CPU逐像素搬运，不缩短总线时序。

DMA初始化由调度器运行后的GUI任务通过`GUI_DisplayInit`执行，IRQ入口位于
`Core/Src/stm32f4xx_it.c`的USER CODE区；无需在CubeMX重复添加同一条DMA流。
USART2保留DMA1 Stream5/6。DMA2 Stream0由显示独占，后续外设不得重复占用。
依据[ST RM0090](https://www.st.com.cn/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)，
内存到内存不使用硬件DBM/循环模式；这里的双缓冲是LVGL轮换两块普通SRAM绘图缓冲。

中间块通过LVGL的flush_wait回调等待，末块主动收尾；中断只续传/发信号量，LVGL始终由GUI任务调用。
单笔累计等待上限100ms，错误/超时先停流再归还缓冲，并在100ms后请求完整重绘；无法停流时保护停机。
OLED的MS包含整帧绘图与末块发送；等待统计仅计GUI实际等待DMA的时间，不能当作CPU占用率。

现有测量/质检、设置/记录、相机网络与云控业务保持；存储仍为内存，外部Flash尚未接入。
H8.3 DMA代码/ARM编译及两项专用主机测试通过，当前主页/设置/模拟测量也已覆盖；
历史gui_migration/gui_touch仍有图像分配断言/旧四导航失配，不能宣称历史整套通过。
下一步下载正常Debug固件，切页/滑动/测量，观察OLED FPS/MS及ERR不增长，并检查触摸/通信共存。
尚未下载，不新增实测帧率、CPU/栈峰值或长期共存结论。阶段证据、完整接线与后续路线以
工程根目录`PROJECT_PROGRESS.md`为准。

## 滚动与返回行为（5.75 / H8.4，2026-10-26）

设置、历史等菜单正文以及系统信息的任务列表/设备页启用LVGL原生弹性与惯性。
有滚动范围的区域在顶部向下拉、底部向上推均可阻尼拖出，按住保持、松手回位；
惯性根据最近手指位移和时间计算，快划比慢拖走得远，再次按下可接手拖动。
任务表头/标签固定，滚动不向外层页面传递；未溢出的静态标签仍不滚动。
按用户最新要求，关闭设置/历史恢复直接返回主页，取消5.73新增的关闭侧滑；
打开时原180ms进入动画和滚动回弹/惯性继续保留。关闭时停止菜单滚动，
先同步最新主页，再隐藏菜单、完成屏外定位，避免重开时多刷整页。
编译/回归与实机状态以固定进度5.75为准，原5.73截图仅记录当时行为。

## 系统信息（H8.4，2026-10-26）

设置中的原“设备信息”已合并为“系统信息”，包含概览、任务、内存、设备。
任务表可上下拖动，表头固定，点任务查看详情，返回先回任务表。
页面为480×320，统一8px卡片间距及221px双列宽度，专用字体在Flash，不增加绘图缓冲。
5.72按最新要求优化：任务表填满208px正文，6个任务一屏可见，更多任务仍可拖动；
概览用任务数替换运行时间，CPU显示蓝色比例条/STM32F407VGT6名称，右侧为真实调度器图标。
5.74按用户澄清保留内存卡片原字体、文案、单位和左右两行，仅将条上移2px、
说明行下移4/6px；真实字形测得条/行/底部间距均9px，原底部15px已重新分配。
最终图`build/H8_DMA_GUI/system-memory-spacing.png`；像素平移核验报告
`system-memory-spacing-audit.json`证明原样式保持，三列/放大字号草稿已撤回。
最新固件清单为`build/Debug/h8_memory_spacing_firmware_manifest.json`，待下载实屏验收。
主页面字体保持，只补新标题字形；本页字体可运行`tools/generate_system_fonts.py`重建。

`configGENERATE_RUN_TIME_STATS=1`，TIM5内部1MHz时钟经64位累计用于任务计时。
TIM5现已占用，不在CubeMX重复配置；没有占引脚/新增中断，也不改原TIM6/HAL时基。
GUI循环每秒轻量读取总时间/IDLE时间，离开页面仍维护最近CPU结果，进入直接显示；
第一次采样用启动以来真实累计值，后续用相邻约1秒差值，不虚构尚无时长时的数据。
可见页从上次刷新起计满1秒更新，各任务CPU也改为相邻采样差值；计数可能包含IRQ归属，
不等于剔除中断后的指令时间。栈每秒采样：当前GUI读PSP，其他任务读最近切换快照，
历史最小余量来自FreeRTOS栈填充值扫描，均转换为字节/KiB。

RTOS堆显示当前/最低余量与最大空闲块，LVGL显示52KiB整池、当前余量/最大块和分配峰值；
52KiB整池已用量包括分配器管理开销。SRAM/CCM/Flash的预留取当前链接符号，不写死旧数字。
固定快照最多12任务，超容量明确提示。全部采集/更新只在GUI任务执行，隐藏时不扫描任务栈。

ARM及系统信息UI/运行统计模型/原DMA三项电脑专项已通过。实际LVGL对照图和日志位于
`build/H8_DMA_GUI/system-layout-optimized.png`及`build/Debug/h8_system_layout_test.log`；
5.71旧预览对照图保留为历史，5.72四视图及设备上下端已实际看图检查，0/36.8/100%蓝条像素检查通过。
图内运行值是固定测试数据，不能当作实屏/真实CPU证据。
下一步下载正常Debug，进入系统信息直接验CPU并观察每秒更新，验列表/详情/四标签及测量触摸相机共存，
记录板上CPU、各任务最小余栈、堆最低余量与LVGL峰值。完整状态以固定进度文件为准。
