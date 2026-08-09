# STM32F407 模块移植项目总结

## 项目简介

本项目是一个基于 **STM32F407VGT6** 的标准库工程，用于整理和移植常用基础模块，当前已经集成了以下内容：

- `Delay` 阻塞式延时模块
- `LED` 指示灯控制模块
- `Key` 按键扫描模块
- `OLED` 软件 I2C 显示驱动模块
- `CMSIS + STM32F4xx StdPeriph` 基础工程框架

工程使用 **Keil MDK** 管理，目标器件为 `STM32F407VGTx`，预处理宏为 `USE_STDPERIPH_DRIVER`、`STM32F40_41xxx`。

## 工程配置概览

| 项目 | 说明 |
| --- | --- |
| MCU | STM32F407VGT6 |
| 工程文件 | `project.uvprojx` |
| 开发框架 | CMSIS + STM32F4xx Standard Peripheral Library |
| 标准库版本 | V1.8.1 |
| 时钟源 | `HSE_VALUE = 8MHz` |
| 系统时钟 | `SystemCoreClock = 168MHz` |
| 开发工具 | Keil uVision / MDK-ARM |
| 当前输出文件 | `Output/Objects/Project.axf` |

时钟配置来自 `Core/system_stm32f4xx.c`，在 `STM32F40_41xxx` 条件下使用：

- `PLL_M = 8`
- `PLL_N = 336`
- `PLL_P = 2`
- `PLL_Q = 7`

即：`8MHz / 8 * 336 / 2 = 168MHz`

## 当前工程功能

当前 `Core/main.c` 中实现的是一个简单的 OLED 显示演示：

1. 上电后初始化 OLED。
2. 循环执行全屏填充显示，并点亮 `LED1`。
3. 延时 300 ms 后熄灭 `LED1`。
4. OLED 切换为测试画面，显示接线提示 `PB8 SCL / PB9 SDA` 和地址 `0x78`。
5. 再延时 700 ms，进入下一轮循环。

测试画面内容包括：

- 屏幕外框
- 两条对角线
- 字符串 `"OLED"`
- 接线提示
- I2C 地址提示

## 目录结构说明

```text
.
├─Core/            主程序、启动文件、中断入口、系统时钟配置
├─Hardware/        板级硬件模块：LED、Key、OLED
├─System/          通用系统模块：Delay
├─UserConfig/      标准库配置头文件
├─Drivers/         CMSIS 和 STM32F4 标准外设库
├─Output/          编译输出目录
├─DebugConfig/     调试配置
└─project.uvprojx  Keil 工程文件
```

说明：

- `App/` 目录目前为空，主程序已经位于 `Core/`。
- `Output/` 中包含编译生成的目标文件、列表文件和 `axf` 可执行映像。

## 模块说明

### 1. Delay 模块

位置：

- `System/Delay.c`
- `System/Delay.h`

特点：

- 基于 `SysTick` 实现阻塞式延时
- 提供 `Delay_us()`、`Delay_ms()`、`Delay_s()`
- `Delay_us()` 已考虑 `SysTick` 24 位计数器上限，长延时会自动分段执行

适用场景：

- 裸机实验
- 简单外设初始化等待
- 软件时序驱动

### 2. LED 模块

位置：

- `Hardware/LED.c`
- `Hardware/LED.h`

功能：

- 初始化 PA1、PA2 为推挽输出
- 提供 `LED1_ON()`、`LED1_OFF()`、`LED2_ON()`、`LED2_OFF()`
- 提供 `LED1_SetMode()`、`LED2_SetMode()` 和 `LED_Tick()`，支持多种闪烁模式

引脚定义：

| 设备 | 引脚 | 说明 |
| --- | --- | --- |
| LED1 | `PA1` | 低电平点亮 |
| LED2 | `PA2` | 低电平点亮 |

### 3. Key 模块

位置：

- `Hardware/Key.c`
- `Hardware/Key.h`

功能：

- 支持 4 个按键扫描
- 内部状态机支持以下事件标志：
  - 按下 `KEY_DOWN`
  - 松开 `KEY_UP`
  - 单击 `KEY_SINGLE`
  - 双击 `KEY_DOUBLE`
  - 长按 `KEY_LONG`
  - 长按连发 `KEY_REPEAT`
  - 保持按下 `KEY_HOLD`

引脚定义：

| 按键 | 引脚 | 上下拉 | 有效电平 |
| --- | --- | --- | --- |
| KEY1 | `PB1` | 上拉 | 低电平按下 |
| KEY2 | `PB11` | 上拉 | 低电平按下 |
| KEY3 | `PB13` | 下拉 | 高电平按下 |
| KEY4 | `PB15` | 下拉 | 高电平按下 |

时序参数：

- 双击判定时间：`200`
- 长按判定时间：`2000`
- 连发周期：`100`

说明：

- `Key_Tick()` 中已经实现按键状态机。
- `Key_Check()` 当前为空函数，说明按键事件对外接口还未完全整理好。

### 4. OLED 模块

位置：

- `Hardware/OLED.c`
- `Hardware/OLED.h`
- `Hardware/OLED_Data.c`
- `Hardware/OLED_Data.h`

硬件连接：

| 信号 | 引脚 | 说明 |
| --- | --- | --- |
| SCL | `PB8` | 软件 I2C 时钟 |
| SDA | `PB9` | 软件 I2C 数据 |

默认参数：

| 参数 | 当前值 | 说明 |
| --- | --- | --- |
| `OLED_I2C_ADDRESS` | `0x78` | 常见 SSD1306 写地址 |
| `OLED_POWER_ON_DELAY_MS` | `100` | 上电稳定等待时间 |
| `OLED_I2C_DELAY_US` | `1` | 软件 I2C 延时 |
| `OLED_COLUMN_OFFSET` | `0` | 列偏移，SH1106 常见为 2 |

OLED 模块已具备的能力：

- 屏幕缓存管理
- 全屏更新与局部更新
- 清屏、区域清屏、区域反显
- 显示 ASCII 字符、字符串、整数、浮点数、十六进制、二进制
- `OLED_Printf()` 格式化输出
- 显示图片
- 绘制点、线、矩形、三角形、圆、椭圆、圆弧
- 支持中文点阵字库

字符集说明：

- `OLED_Data.h` 当前启用了 `OLED_CHARSET_UTF8`
- 如需显示中文，字模需要在 `OLED_Data.c` 的 `OLED_CF16x16[]` 中提前定义

## 主程序行为总结

当前主程序的逻辑比较简单，重点在于验证 OLED 驱动是否工作正常：

```c
int main(void)
{
    OLED_Init();

    while (1)
    {
        LED1_ON();
        OLED_DrawRectangle(0, 0, 128, 64, OLED_FILLED);
        OLED_Update();
        Delay_ms(300);

        LED1_OFF();
        OLED_ShowTestPattern();
        Delay_ms(700);
    }
}
```

这个主循环可以作为后续功能开发的基础模板，例如加入：

- 按键翻页
- 菜单界面
- 传感器数据显示
- 调试信息输出

## 编译与使用方法

### 1. 打开工程

使用 Keil 打开：

- `project.uvprojx`

### 2. 编译

直接编译目标 `Project` 即可。

当前工程构建记录显示：

- `0 Error(s), 0 Warning(s)`
- 产物：`Output/Objects/Project.axf`
- 程序体积：`Code=3408 RO-data=2808 RW-data=20 ZI-data=2052`

### 3. 下载运行

使用 ST-Link 下载到开发板后，可观察：

- OLED 在全屏填充和测试画面之间切换
- `LED1` 同步闪烁

## 已知注意事项

### 1. `main.c` 中未调用 `LED_Init()`

当前主程序直接调用了 `LED1_ON()` 和 `LED1_OFF()`，但没有先执行 `LED_Init()`。

这意味着：

- 如果 PA1、PA2 没有被别处初始化，LED 可能无法按预期工作
- 建议在 `main()` 开始处补上 `LED_Init()`

### 2. `Key_Check()` 尚未完成

虽然 `Key_Tick()` 已实现完整按键状态机，但 `Key_Check()` 目前是空函数，说明：

- 按键事件已经在内部产生
- 但对上层应用的读取接口还没有封装完成

后续建议补充：

- 查询式接口
- 清标志接口
- 回调式事件派发接口

### 3. OLED 参数宏与源码仍有不完全统一的地方

`Hardware/OLED.h` 中已经预留了：

- `OLED_I2C_ADDRESS`
- `OLED_COLUMN_OFFSET`

但在 `Hardware/OLED.c` 中：

- OLED 地址仍直接写死为 `0x78`
- 列偏移逻辑仍保留在注释代码中，没有真正使用 `OLED_COLUMN_OFFSET`

这说明当前驱动虽然能用，但还可以再做一次“参数化整理”，提升可移植性。

### 4. Delay 为阻塞式实现

当前 `Delay` 模块适合裸机小项目和演示程序，但不适合：

- 多任务并发
- 高实时性调度
- RTOS 任务环境下的大量阻塞等待

## 后续优化建议

建议下一步按以下顺序继续完善：

1. 在 `main()` 中补充 `LED_Init()`、`Key_Init()`
2. 为 `Key` 模块补全对外事件读取接口
3. 将 OLED 地址和列偏移彻底改为宏配置
4. 将 `main.c` 改造成按键驱动的页面切换示例
5. 根据需要开启 `hex` 文件生成，方便量产下载或第三方烧录工具使用

## 总结

这个工程已经具备了一个标准 STM32F407 裸机项目的基本骨架，尤其适合作为后续实验和模块移植的起点。当前最完整、最有复用价值的部分是 `OLED` 显示驱动，`Delay` 和 `LED` 也已经可以直接投入使用；`Key` 模块的内部扫描框架已完成，但对外接口还需要再整理一轮。

如果后续继续扩展，本工程非常适合作为：

- STM32F407 学习模板
- 外设移植基础工程
- OLED 界面实验平台
- 裸机功能验证工程
