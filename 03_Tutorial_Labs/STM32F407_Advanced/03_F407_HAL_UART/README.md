# STM32F407 HAL 串口入门工程

这是一个面向 HAL 初学者的 STM32F407 串口基础工程。工程由 STM32CubeMX 生成，使用 **USART1**，已经完成系统时钟、GPIO 复用和串口参数的初始化。

> 当前代码只完成了初始化，`while (1)` 还是空的。烧录原始工程后，串口不会主动输出内容，这是正常现象。本文后面提供了可以直接加入工程的发送和回显示例。

## 1. 工程概况

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F407VGT6 |
| 封装 | LQFP100 |
| 库 | STM32CubeF4 HAL（工程配置为 V1.28.3） |
| 系统主频 | 168 MHz |
|外部晶振 | 8 MHz HSE |
| 串口 | USART1，异步收发 |
| 串口参数 | 115200，8 数据位，1 停止位，无校验，无流控（115200 8N1） |
| 构建系统 | CMake + Ninja + GNU Arm Embedded Toolchain |
| 下载/调试 | J-Link，SWD 接口 |

本工程的 `.ioc` 文件按 **STM32F407VGT6 + 8 MHz 外部晶振**配置。如果你的芯片型号或晶振频率不同，需要先在 STM32CubeMX 中修改配置，否则程序可能无法正常运行。

## 2. 引脚分配

### USART1 与 USB 转串口模块

| STM32F407 引脚 | 功能 | 连接到 USB 转串口模块 |
| --- | --- | --- |
| PA9 | USART1_TX，单片机发送 | RXD |
| PA10 | USART1_RX，单片机接收 | TXD |
| GND | 公共地 | GND |

接线要点：

- 串口的 TX 和 RX 要交叉连接：`PA9(TX) -> 模块 RXD`，`PA10(RX) <- 模块 TXD`。
- 两边必须共地。
- 使用 **3.3 V TTL 电平**的 USB 转串口模块。不要把 PA9/PA10 直接接到传统 RS-232 接口，RS-232 电压不兼容，可能损坏芯片。
- 不要同时由开发板和 USB 转串口模块的 VCC 给板子供电。通常只连接 TXD、RXD、GND 即可，开发板单独供电。
- PA9、PA10 在程序中被配置成复用功能 `AF7`，分别连接到 USART1_TX 和 USART1_RX。

### J-Link/SWD 下载调试

| STM32F407 引脚 | SWD 功能 | J-Link |
| --- | --- | --- |
| PA13 | SWDIO | SWDIO |
| PA14 | SWCLK | SWCLK |
| GND | 公共地 | GND |
| 3.3 V | 目标电压参考 | VTref |
| NRST（建议连接） | 复位 | RESET |

`VTref` 用来让 J-Link 识别目标板电平，通常不是用来给整块开发板供电的。

### 时钟引脚

| 引脚 | 功能 |
| --- | --- |
| PH0-OSC_IN | 8 MHz 外部晶振输入 |
| PH1-OSC_OUT | 8 MHz 外部晶振输出 |

## 3. 程序是怎样启动的

程序入口在 `Core/Src/main.c`，关键流程如下：

```c
HAL_Init();                 // 初始化 HAL、SysTick 等基础功能
SystemClock_Config();       // 8 MHz HSE 经 PLL 得到 168 MHz 系统时钟
MX_GPIO_Init();             // 打开用到的 GPIO 端口时钟
MX_USART1_UART_Init();      // 初始化 USART1

while (1)
{
    // 在这里放循环执行的应用代码
}
```

串口初始化位于 `Core/Src/usart.c`，生成了一个全局句柄：

```c
UART_HandleTypeDef huart1;
```

以后调用 HAL 串口函数时，通过 `&huart1` 告诉 HAL 操作 USART1。

## 4. 从标准库过渡到 HAL

如果你用过 STM32 标准外设库，可以先这样理解二者的对应关系：

| 以前使用标准库时 | HAL 中的做法 |
| --- | --- |
| `RCC_AHB1PeriphClockCmd(...)` | `__HAL_RCC_GPIOA_CLK_ENABLE()` |
| `GPIO_Init(GPIOA, &GPIO_InitStruct)` | `HAL_GPIO_Init(GPIOA, &GPIO_InitStruct)` |
| `USART_Init(USART1, &USART_InitStruct)` | 填写 `huart1.Init` 后调用 `HAL_UART_Init(&huart1)` |
| `USART_SendData()` 并等待发送标志 | `HAL_UART_Transmit()` |
| 检查 RXNE 后调用 `USART_ReceiveData()` | `HAL_UART_Receive()`，或使用中断/DMA 接收 |

HAL 的主要特点是使用“句柄”。`huart1` 不只表示 USART1，还保存初始化参数和驱动当前的状态。CubeMX 把底层时钟、GPIO 复用等代码生成好了，新手可以先学会调用 HAL API，再逐步阅读其底层实现。

## 5. 第一个实验：发送字符串

打开 `Core/Src/main.c`，在 `/* USER CODE BEGIN 2 */` 与 `/* USER CODE END 2 */` 之间加入：

```c
const uint8_t hello[] = "Hello, STM32 HAL!\r\n";
HAL_UART_Transmit(&huart1, hello, sizeof(hello) - 1, HAL_MAX_DELAY);
```

烧录并复位后，串口助手设置为：

- 波特率：115200
- 数据位：8
- 停止位：1
- 校验位：None（无校验）
- 流控：None（无流控）

应该能看到一次 `Hello, STM32 HAL!`。

函数参数含义：

```c
HAL_UART_Transmit(
    &huart1,          // 使用哪个串口
    hello,            // 要发送的数据地址
    sizeof(hello)-1,  // 数据长度，不发送字符串末尾的 \0
    HAL_MAX_DELAY     // 最长等待时间；这里表示一直等到发送完成
);
```

这是阻塞式发送：CPU 会等到数据发完才继续执行。它很适合入门和少量调试信息，大量数据或实时任务通常改用中断或 DMA。

## 6. 第二个实验：收到什么就发回什么

先在 `main()` 的 `/* USER CODE BEGIN 1 */` 区域定义接收变量：

```c
uint8_t rx_byte;
```

然后在 `while (1)` 内的 `/* USER CODE BEGIN 3 */` 区域加入：

```c
if (HAL_UART_Receive(&huart1, &rx_byte, 1, HAL_MAX_DELAY) == HAL_OK)
{
    HAL_UART_Transmit(&huart1, &rx_byte, 1, HAL_MAX_DELAY);
}
```

重新编译、烧录后，在串口助手中发送字符，STM32 会原样发回。这里的 `HAL_UART_Receive()` 同样是阻塞式的：没有收到数据时，程序会一直停在此处等待。

若希望主循环还能做别的事情，可把超时改小，例如：

```c
if (HAL_UART_Receive(&huart1, &rx_byte, 1, 10) == HAL_OK)
{
    HAL_UART_Transmit(&huart1, &rx_byte, 1, 100);
}
```

其中 `10` 和 `100` 的单位都是毫秒。

## 7. 编译、烧录与调试

### 所需软件

- CMake 3.22 或更高版本
- Ninja
- GNU Arm Embedded Toolchain（命令前缀为 `arm-none-eabi-`）
- J-Link Software and Documentation Pack
- VS Code（可选）
- VS Code 扩展：CMake Tools、C/C++、Cortex-Debug（使用 VS Code 调试时）

确保下面这些命令能在终端中运行：

```powershell
cmake --version
ninja --version
arm-none-eabi-gcc --version
```

### 命令行编译

在工程根目录执行：

```powershell
cmake --preset debug-armgcc --fresh
cmake --build --preset build-debug
```

编译产物位于 `build/Debug`，主固件是 `.elf` 文件。

### VS Code 编译

按 `Ctrl+Shift+B`，默认会依次执行 `Configure` 和 `Build` 任务。也可以从“终端 -> 运行任务”中选择：

- `Configure`：重新生成 CMake 构建目录；
- `Build`：配置并编译 Debug 固件；
- `Clean`：清理本工程的构建产物；
- `Rebuild All`：清理后重新编译；
- `Flash`：先编译，再通过 J-Link/SWD 烧录。

### 烧录和调试前的路径配置

仓库中的 VS Code 配置带有原作者电脑上的 J-Link 安装路径，例如：

```text
D:/Program Files/SEGGER/JLink_V958/
```

如果你的安装位置不同，需要修改：

- `.vscode/launch.json` 中的 `serverpath`；
- `.vscode/settings.json` 中的 `cortex-debug.JLinkGDBServerPath.windows`；
- `.vscode/scripts/flash.cmd` 中的 `JLINK_EXE`。

调试配置还通过环境变量 `ARM_GCC_ROOT` 查找 GDB 和 objdump。它应指向 Arm GNU 工具链根目录，例如该目录下面应存在 `bin/arm-none-eabi-gdb.exe`。

接好 J-Link 后，可运行 `Flash` 任务进行烧录；按 `F5` 可启动 `Debug (Cortex-Debug J-Link)`，程序会运行到 `main()`。

## 8. CubeMX 中的配置位置

双击 `03_F407_HAL_UART.ioc`，或在 VS Code 中运行 `CubeMX` 任务，可以查看图形化配置：

1. 在 Pinout 页面将 PA9 设为 `USART1_TX`，PA10 设为 `USART1_RX`；
2. 在 USART1 参数中选择 Asynchronous，配置 115200、8N1、无流控；
3. RCC 使用 8 MHz HSE；
4. Clock Configuration 将 SYSCLK 配置为 168 MHz；
5. SYS 调试接口使用 Serial Wire，保留 PA13/PA14；
6. 生成代码的工具链为 CMake。

修改 `.ioc` 并重新生成代码时，自写代码尽量放在 `USER CODE BEGIN` 和 `USER CODE END` 之间，CubeMX 才会保留它。例如：

```c
/* USER CODE BEGIN 2 */
// 在这里写初始化后只执行一次的代码
/* USER CODE END 2 */
```

## 9. 主要目录和文件

```text
.
├─ Core/
│  ├─ Inc/                 主要头文件
│  └─ Src/
│     ├─ main.c            程序入口、系统时钟、主循环
│     ├─ usart.c           USART1 参数、GPIO 复用与外设时钟
│     ├─ gpio.c            GPIO 端口时钟初始化
│     └─ stm32f4xx_it.c    中断服务函数
├─ Drivers/
│  ├─ CMSIS/               Cortex-M4 和 STM32F407 设备定义
│  └─ STM32F4xx_HAL_Driver/ HAL 驱动源码
├─ cmake/                  CMake 工具链及 CubeMX 生成的源文件列表
├─ 03_F407_HAL_UART.ioc    STM32CubeMX 工程配置
├─ STM32F407xx_FLASH.ld    Flash/RAM 链接脚本
├─ startup_stm32f407xx.s   启动文件和中断向量表
└─ CMakeLists.txt          CMake 工程入口
```

刚开始学习时，建议优先阅读：

1. `Core/Src/main.c`：理解初始化顺序和主循环；
2. `Core/Src/usart.c`：观察 CubeMX 如何配置串口；
3. `Core/Inc/usart.h`：查看 `huart1` 如何提供给其他文件使用；
4. `Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h`：需要时查询 HAL UART API。

## 10. 常见问题

### 串口助手什么也收不到

原始工程不会主动发送数据，请先加入“发送字符串”示例。然后依次检查：

1. TX/RX 是否交叉连接；
2. 是否共地；
3. 串口助手是否为 115200 8N1、无流控；
4. 是否打开了正确的电脑串口号；
5. USB 转串口模块是否为 3.3 V TTL 电平；
6. 程序是否成功烧录并复位运行；
7. 开发板是否确实装有 8 MHz 外部晶振。

### 收到乱码

通常是波特率或系统时钟不一致。先确认串口助手为 115200，再确认实际外部晶振是 8 MHz。如果板载晶振不是 8 MHz，需要在 CubeMX 中按真实频率重新配置时钟。

### 能发送，不能接收

重点检查 USB 转串口模块的 TXD 是否接到 PA10，以及两边是否共地。还要确认代码确实调用了 `HAL_UART_Receive()`。

### 程序进入 `Error_Handler()`

工程初始化失败时会关闭中断并停在 `Error_Handler()`。可以用调试器打断点，观察是 `SystemClock_Config()` 还是 `MX_USART1_UART_Init()` 调用失败。最常见的硬件原因是外部晶振配置与实际板卡不一致。

### 修改代码后被 CubeMX 覆盖

CubeMX 只保证保留 `USER CODE BEGIN/END` 标记内的内容。若要长期维护自写模块，也可以新建独立的 `.c/.h` 文件，并把源文件加入顶层 `CMakeLists.txt` 的 `target_sources()`。

## 11. 下一步建议

完成阻塞式收发后，可以按以下顺序继续学习：

1. 使用有限超时，避免主循环永久阻塞；
2. 使用 `HAL_UART_Receive_IT()` 学习串口接收中断；
3. 学习 `HAL_UART_RxCpltCallback()` 回调函数；
4. 用环形缓冲区处理连续、不定长的数据；
5. 使用 DMA 接收，以及空闲线中断处理不定长数据帧；
6. 最后再学习 `printf` 重定向，避免一开始被 C 库和半主机配置分散注意力。

