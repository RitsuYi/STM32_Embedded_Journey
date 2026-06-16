# STM32 FreeRTOS 01_LED

这是一个基于 STM32CubeMX 生成的 STM32F103C8 FreeRTOS 入门工程，当前工程已经完成 HAL、GPIO、FreeRTOS 和 Keil MDK-ARM 工程配置。

## 工程概览

- MCU: STM32F103C8, Cortex-M3, LQFP48
- 工具链: MDK-ARM V5.32 / Keil uVision5
- 中间件: FreeRTOS, CMSIS-RTOS v1
- 系统时钟: HSE + PLL x9, SYSCLK 72 MHz
- HAL 时基: TIM4
- 板载 LED: PC13, 工程标签为 `BoardLED`
- 输出文件: `MDK-ARM/Project/Project.hex`

## 目录结构

```text
Project/
├── Core/
│   ├── Inc/                  # 用户头文件与 HAL/FreeRTOS 配置
│   └── Src/                  # main.c、GPIO、FreeRTOS 初始化等源码
├── Drivers/                  # STM32F1 HAL 与 CMSIS 驱动
├── Middlewares/
│   └── Third_Party/FreeRTOS/ # FreeRTOS 内核与 CMSIS-RTOS 适配层
├── MDK-ARM/                  # Kei l 工程文件与构建产物
└── Project.ioc               # STM32CubeMX 配置文件
```

## 关键文件

- `Core/Src/main.c`
  - 完成 HAL 初始化、系统时钟配置、GPIO 初始化、FreeRTOS 初始化，并启动调度器。
- `Core/Src/gpio.c`
  - 使能 GPIOC/GPIOD/GPIOA 时钟。
  - 将 `BoardLED` 配置为推挽输出，默认输出 `GPIO_PIN_SET`。
- `Core/Src/freertos.c`
  - 创建 `defaultTask`，优先级为 `osPriorityNormal`，栈大小为 128。
  - 当前默认任务循环中只执行 `osDelay(1)`，还没有加入 LED 翻转逻辑。
- `Core/Inc/FreeRTOSConfig.h`
  - `configTICK_RATE_HZ` 为 1000。
  - `configTOTAL_HEAP_SIZE` 为 4096。
  - 同时开启静态分配与动态分配支持。

## 构建方法

1. 打开 `MDK-ARM/Project.uvprojx`。
2. 在 Keil uVision5 中选择目标 `Project`。
3. 执行 Build 或 Rebuild。
4. 构建完成后，固件输出位于：

```text
MDK-ARM/Project/Project.hex
```

当前目录中已有一次构建记录，`MDK-ARM/Project/Project.build_log.htm` 显示：

```text
0 Error(s), 0 Warning(s)
Program Size: Code=6524 RO-data=352 RW-data=124 ZI-data=6636
```

## 下载与运行

可使用 ST-LINK、DAPLink 或其他支持 STM32F103C8 的调试器，将 `Project.hex` 下载到开发板。

当前程序启动后会进入 FreeRTOS 调度器，但默认任务中尚未控制 LED。如果需要实现 LED 闪烁，可在 `Core/Src/freertos.c` 的 `StartDefaultTask` 用户代码区中加入类似逻辑：

```c
for (;;)
{
  HAL_GPIO_TogglePin(BoardLED_GPIO_Port, BoardLED_Pin);
  osDelay(500);
}
```

## 维护说明

- 建议把用户代码写在 `/* USER CODE BEGIN ... */` 与 `/* USER CODE END ... */` 之间，避免 CubeMX 重新生成代码时被覆盖。
- 修改引脚、时钟或 FreeRTOS 配置时，优先通过 `Project.ioc` 在 STM32CubeMX 中调整后重新生成。
- `MDK-ARM/Project/` 下包含构建产物，重新编译时会更新。
