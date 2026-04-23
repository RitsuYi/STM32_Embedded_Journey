# STM32F407 四轮闭环小车工程说明

本 README 基于当前项目文件夹中的源码、Keil 工程配置、构建产物和辅助文档整理，目标不是“给人快速看一眼”，而是“让另一个 AI 或新接手的开发者只看这一个文件，就能尽快理解这个工程的结构、功能、约束、接口和当前状态”。

## 1. 项目一句话

这是一个基于 `STM32F407VGTx`、使用 `CMSIS + STM32F4xx StdPeriph Library` 的四轮小车控制工程。  
它已经实现了：

- 四路编码器测速
- 四个直流电机的速度闭环控制
- 整车状态机控制：前进、后退、原地左转、原地右转、停止
- OLED 实时状态显示
- 电池电压 ADC 采样
- 蓝牙串口协议、在线调 PID、在线改目标速度、遥测曲线输出
- MPU6050 姿态采样
- HMC5883L 磁航向采样与校准框架
- MPU6050 + HMC5883L 的 yaw 融合

它目前**没有真正实现**的内容也很重要：

- 航向辅助/航向纠偏还没有真正接入电机控制闭环
- 当前真正参与运动控制的是“四路速度环”，不是“姿态环/航向环”
- 蓝牙里保留了 `HEADING` 控制入口，但在 `Motor.c` 里仍是占位实现

## 2. 当前工程的真实定位

如果你是 AI，需要把这个工程理解成下面这件事：

> 这是一个“已经能跑起来的 STM32F407 四轮闭环小车底盘工程”，重点在电机速度闭环、在线调参、实时观测和 IMU/磁力计采样；姿态融合已经做了，但还处于“可观测、未闭环接入整车转向控制”的阶段。

因此：

- 这不是一个纯外设例程
- 这不是一个 RTOS 工程
- 这不是一个平衡车工程
- 这不是一个完整自动驾驶工程
- 这是一个偏“底盘控制 + 调参平台 + 传感器观测”的工程

## 3. 构建与工程配置

### 3.1 基本信息

| 项目 | 当前值 |
| --- | --- |
| MCU | `STM32F407VGTx` |
| 工程文件 | `project.uvprojx` |
| 开发环境 | `Keil MDK / uVision` |
| 外设库 | `STM32F4xx StdPeriph Library V1.8.1` |
| 预处理宏 | `USE_STDPERIPH_DRIVER`, `STM32F40_41xxx` |
| 优化等级 | `Optim=1` |
| 输出目录 | `Output/Objects/` |
| 固件输出 | `Output/Objects/Project.axf` |
| Hex 文件 | 当前未生成，`CreateHexFile=0` |

### 3.2 当前编译结果

从现有 `Output/Objects/Project.build_log.htm` 可见，最近一次构建结果为：

- `0 Error(s), 0 Warning(s)`
- `Program Size: Code=33058 RO-data=3090 RW-data=328 ZI-data=7640`

### 3.3 时钟

本工程运行时系统时钟按 `Core/system_stm32f4xx.c` 的 `STM32F40_41xxx` 配置工作：

- `SystemCoreClock = 168000000`
- 常用推导：`HSE = 8MHz`
- 常见 PLL 组合：`PLL_M=8`, `PLL_N=336`, `PLL_P=2`, `PLL_Q=7`

同时，`BoardConfig.h` 里假定：

- `BOARD_APB1_TIMER_CLOCK_HZ = 84000000`

这与 STM32F407 在 168MHz 主频下、APB1 定时器倍频后的常见配置一致。

## 4. 目录结构

```text
.
├─ Core/              主入口、系统时钟、异常和中断入口
├─ App/               应用层任务与 PID 参数管理
├─ System/            通用系统模块：阻塞延时、1ms 调度器
├─ Hardware/          硬件驱动与板级功能
├─ UserConfig/        板级统一配置、StdPeriph 配置头
├─ Drivers/           CMSIS + STM32F4xx StdPeriph 驱动
├─ DebugConfig/       调试配置
├─ Output/            Keil 构建输出
├─ .vscode/           VSCode IntelliSense 和日志
├─ project.uvprojx    Keil 工程
├─ 蓝牙调试说明.MD     蓝牙调参与协议说明文档
├─ test.md            关于速度环现象的分析记录
└─ keilkill.bat       清理 Keil 输出目录的批处理
```

### 4.1 当前真正活跃的源码

当前构建中最关键的业务源码是：

- `Core/main.c`
- `System/Timer.c`
- `App/AppTask.c`
- `App/PID.c`
- `Hardware/Encoder.c`
- `Hardware/Motor.c`
- `Hardware/PWM.c`
- `Hardware/Key.c`
- `Hardware/ADC_Bat.c`
- `Hardware/Serial.c`
- `Hardware/BlueSerial.c`
- `Hardware/BoardI2C.c`
- `Hardware/OLED.c`
- `Hardware/MPU6050.c`
- `Hardware/HMC5883L.c`
- `Hardware/IMU_Fusion.c`

### 4.2 存在但当前不是主线的内容

- `Hardware/LED.c` 已加入工程并会编译，但 `main.c` 当前没有调用 `LED_Init()` 或 `LED_Tick()`，属于保留模块
- `Drivers/STM32F4xx_StdPeriph_Driver/` 里加入了大量标准库源文件，但业务层实际上只直接依赖其中一部分
- 旧 `README.md` 内容和当前代码已经不一致，尤其是按键数量、项目目标、OLED 用法等

## 5. 板级总览

`UserConfig/BoardConfig.h` 是整个工程的“总配置入口”，你应该把它当成最重要的板级文档。

它负责集中定义：

- 引脚映射
- 定时器和外设实例
- 采样周期
- PID 默认参数
- 运动目标速度
- 编码器参数
- I2C/蓝牙/OLED/ADC/IMU 参数
- 航向融合参数

它的命名约定也很重要：

- `_HZ`：频率，Hz
- `_MS`：时间，ms
- `_RPM`：转速，rpm
- `_MV`：电压，mV
- `_DPS`：角速度，deg/s

如果后续要改板子或迁移硬件，**优先先看 `BoardConfig.h`，不要先改驱动源码里的魔法数字**。

## 6. 启动流程

`Core/main.c` 当前启动顺序如下：

1. `Timer_Init()`
2. `OLED_Init()`
3. `Encoder_Init()`
4. `ADC_Bat_Init()`
5. `MPU6050_Init()`
6. `Motor_Init()`
7. `Key_Init()`
8. `AppTask_Init()`
9. `Motor_SetCarState(CAR_STOP)`
10. `OLED_Task()` 先刷一次屏
11. 主循环中反复执行：
    - `AppTask_Run()`
    - `Timer_RunPendingTasks()`

这意味着：

- 所有周期任务不是在 `while(1)` 里手工延时轮询，而是靠 `Timer` 模块调度
- 蓝牙命令、按键扫描等“事件处理”发生在 `AppTask_Run()`
- 传感器采样、电机控制、OLED 周期刷新等“节拍任务”发生在 `Timer_RunPendingTasks()`

## 7. 调度器与实时节拍

### 7.1 调度策略

`System/Timer.c` 使用 `TIM6` 产生 `1ms` 基础节拍。

中断里只做两件事：

- `g_timerTickMs++`
- 各任务倒计时减一，到期后把“待执行次数”加一

真正的任务函数在主循环中的 `Timer_RunPendingTasks()` 里执行。  
这是一个典型的“中断只记账，主循环做事”的轻量调度结构。

### 7.2 当前周期任务

| 任务 | 周期 | 执行函数 |
| --- | --- | --- |
| 编码器测速 | `20ms` | `Encoder_UpdateSpeed()` |
| MPU6050 采样 | `10ms` | `MPU6050_SampleTask()` |
| 电机 PID 控制 | `20ms` | `Motor_ControlTask()` |
| 电池采样 | `50ms` | `ADC_Bat_SampleTask()` |
| OLED 刷新 | `150ms` | `OLED_Task()` |

### 7.3 相关中断入口

- `TIM6_DAC_IRQHandler()` 在 `Core/stm32f4xx_it.c` 中，内部转调 `Timer_IRQHandler()`
- `USART3_IRQHandler()` 不在 `stm32f4xx_it.c` 里，而是在 `Hardware/Serial.c` 里实现

这点很重要：**串口中断处理逻辑不在标准中断模板文件里，而在串口驱动文件里。**

## 8. 高层架构

可以把整个工程理解成下面这条数据流：

```text
按键 PD3 / 蓝牙命令
        ↓
     AppTask
        ↓
整车状态 / 目标速度 / PID 参数
        ↓
     Motor_ControlTask
        ↑
   Encoder_UpdateSpeed
        ↑
   四路编码器定时器
        ↓
     PWM + 电机方向脚
        ↓
       四个电机
```

传感器链路是另一条并行链：

```text
MPU6050 + HMC5883L
        ↓
  MPU6050_SampleTask
        ↓
   HMC5883L_SampleTask
        ↓
    IMU_Fusion_SampleTask
        ↓
  姿态/航向诊断数据 + OLED显示 + 蓝牙遥测
```

注意：

- 这条 IMU 链路目前**没有真正闭环接到转向控制**
- 它现在主要用于测量、观测、调试和后续扩展准备

## 9. 引脚与外设映射

### 9.1 I2C1 总线

OLED、MPU6050、HMC5883L 共用同一条 `I2C1` 总线：

| 信号 | 引脚 |
| --- | --- |
| SCL | `PB8` |
| SDA | `PB9` |

设备地址使用的是 **8 位写地址写法**：

| 设备 | 地址 |
| --- | --- |
| OLED | `0x78` |
| MPU6050 | `0xD0` |
| HMC5883L | `0x3C` |

### 9.2 蓝牙串口

| 功能 | 引脚 |
| --- | --- |
| `USART3_TX` | `PD8`，连接蓝牙模块 `RX` |
| `USART3_RX` | `PD9`，连接蓝牙模块 `TX` |

串口参数：

- 波特率：`9600`
- 格式：`8N1`

### 9.3 PWM 输出

使用 `TIM4` 四路 PWM：

| 通道 | 引脚 |
| --- | --- |
| CH1 | `PD12` |
| CH2 | `PD13` |
| CH3 | `PD14` |
| CH4 | `PD15` |

当前配置：

- `PSC = 0`
- `ARR = 8399`
- 逻辑占空比上限：`1000`
- PWM 频率约：`84MHz / 8400 = 10kHz`

### 9.4 电机方向控制

| 电机 | IN1 | IN2 |
| --- | --- | --- |
| A | `PE8` | `PE9` |
| B | `PE10` | `PD0` |
| C | `PE12` | `PE13` |
| D | `PE14` | `PE15` |

驱动待机脚：

- `STBY = PB5`

### 9.5 编码器

| 编码器 | 定时器 | 引脚 A | 引脚 B |
| --- | --- | --- | --- |
| A | `TIM8` | `PC6` | `PC7` |
| B | `TIM5` | `PA0` | `PA1` |
| C | `TIM3` | `PA6` | `PA7` |
| D | `TIM1` | `PA8` | `PE11` |

### 9.6 其他

| 功能 | 引脚 |
| --- | --- |
| 按键 | `PD3` |
| 电池 ADC | `PA5 -> ADC1_IN5` |
| LED1 | `PA1` |
| LED2 | `PA2` |

## 10. 运动控制模型

### 10.1 电机编号和车轮位置

工程里有两套概念：

- `MOTOR_A/B/C/D`：驱动层的 4 个电机编号
- `左前/右前/左后/右后`：整车逻辑轮位

它们通过 `BoardConfig.h` 的映射宏统一起来：

- `BOARD_CAR_LEFT_FRONT_MOTOR`
- `BOARD_CAR_RIGHT_FRONT_MOTOR`
- `BOARD_CAR_LEFT_REAR_MOTOR`
- `BOARD_CAR_RIGHT_REAR_MOTOR`

同样，编码器也有“逻辑轮位 -> 实际编码器”的映射。

### 10.2 整车状态机

整车状态枚举：

- `CAR_FORWARD`
- `CAR_BACKWARD`
- `CAR_LEFT`
- `CAR_RIGHT`
- `CAR_STOP`

`Key_Scan()` 检测到单个按键稳定按下时，`AppTask_ProcessKey()` 会调用：

- `Motor_SwitchToNextState()`

状态循环顺序是：

`FORWARD -> BACKWARD -> LEFT -> RIGHT -> STOP -> FORWARD`

### 10.3 各状态如何生成四轮目标速度

- `FORWARD`：左右两侧都用 `+g_moveTargetSpeedRpm`
- `BACKWARD`：左右两侧都用 `-g_moveTargetSpeedRpm`
- `LEFT`：左侧 `-g_turnTargetSpeedRpm`，右侧 `+g_turnTargetSpeedRpm`
- `RIGHT`：左侧 `+g_turnTargetSpeedRpm`，右侧 `-g_turnTargetSpeedRpm`
- `STOP`：四轮目标全为 `0`

这是**原地转向模型**，不是差速曲线转弯模型。

### 10.4 目标速度与方向的关系

要特别注意：

- `Motor_SetMoveTargetSpeedRpm()` 和 `Motor_SetTurnTargetSpeedRpm()` 内部都把负数转成正数
- 也就是说，这两个接口保存的是“目标速度幅值”
- 真正的正负方向由 `CAR_FORWARD / CAR_BACKWARD / CAR_LEFT / CAR_RIGHT` 决定

因此：

- `[CTRL,SPEED,-200]` 不会让车自动改成后退
- 后退要用状态切换：`[STATE,SET,BACKWARD]` 或 `[MODE,BACKWARD]`

## 11. 编码器与速度估计

### 11.1 编码器策略

`Hardware/Encoder.c` 使用的是“定时器计数器居中法”：

- 初始化时把计数器放到中心值
- 每次采样读取当前值与中心值的差，作为本周期增量
- 读取后再把计数器重新写回中心值

这样做的优点：

- 同时适配 16 位和 32 位定时器
- 更适合做固定窗口的“增量测速”
- 方向符号处理更直接

### 11.2 速度估计参数

当前配置：

- 编码器采样周期：`20ms`
- 速度平均窗口：`4` 个样本
- 每输出轴一圈计数：`13 * 4 * 20 = 1040`

所以：

- 速度反馈并不是瞬时值
- 它实际上是最近 `4 * 20ms = 80ms` 窗口的平均速度

### 11.3 左右平均速度

`Encoder_GetLeftSpeedRpm()` 和 `Encoder_GetRightSpeedRpm()` 返回的是：

- 左前、左后平均
- 右前、右后平均

当前主控制没有直接使用它们，而是逐电机读取各自编码器速度。

## 12. PID 设计

### 12.1 当前只有速度环真正生效

`App/PID.c` 当前维护的是“全局速度环默认配置”，再同步到四个电机各自的 `PID_t` 实例。

默认参数来自 `BoardConfig.h`：

- `Kp = 0.10`
- `Ki = 0.05`
- `Kd = 0.00`
- `IntegralLimit = 1200`
- `OutputLimit = 1000`
- `OutputOffset = 0.0`

### 12.2 当前 PID 公式

对每个电机：

```text
Error = Target - Actual
POut = Kp * Error
IOut += Ki * Error
DOut = -Kd * (Actual - Actual1)
Out = POut + IOut + DOut
```

其中：

- 微分项是对测量值做差分，不是对误差做差分
- `IOut` 有积分限幅
- `Out` 有输出限幅
- `OutOffset` 字段存在，但当前默认被加载为 `0.0f`

### 12.3 一个很重要的控制语义

`Motor_ControlTask()` 会把 `PID_Calculate()` 的输出当成**带符号 PWM 命令**：

- 正数：正转
- 负数：反转
- 0：停止

这意味着：

- 即使整车处于 `CAR_FORWARD`
- 如果某个轮子的速度环输出因为误差变化变成负数
- 该轮也会直接进入反向驱动

这对调参行为影响很大，尤其是在接近目标速度时。

## 13. 电机驱动层

### 13.1 `Motor.c` 的职责

`Hardware/Motor.c` 是本项目最核心的业务模块之一，负责：

- PWM 初始化后的四个电机方向控制
- 单个电机正转/反转/停止
- 整车状态管理
- 把整车状态转换成四轮目标转速
- 调用 PID 做四路闭环控制

### 13.2 开环与闭环

- `Motor_SetSpeed()` 是直接驱动模式，不自动开启闭环
- `Motor_CarForward/Backward/TurnLeft/TurnRight()` 会进入闭环
- `Motor_CarStop()` 会退出闭环并停止所有电机

### 13.3 航向辅助当前是占位接口

以下接口虽然存在，但当前仍是占位实现：

- `Motor_SetHeadingAssistEnabled()`
- `Motor_GetHeadingCorrectionRpm()`
- `Motor_IsHeadingCorrectionEnabled()`
- `Motor_IsHeadingAssistEnabled()`

现状是：

- 蓝牙命令 `[CTRL,HEADING,x]` 可以发送
- 状态帧里也保留了 heading 字段
- 但 `Motor.c` 当前始终返回“未启用/无修正”

也就是说：**协议接口已预留，功能尚未接入。**

## 14. 按键模块

`Hardware/Key.c` 当前实现的是**一个物理按键**，不是多键事件系统。

当前特性：

- 引脚：`PD3`
- 上电读取初始状态
- 去抖时间：`20ms`
- `Key_Scan()` 只在“稳定按下沿”返回 `1`

与旧文档不同的是：

- 当前工程没有 4 键扫描
- 当前工程没有双击/长按/连发事件状态机
- 目前只是一个简单的“单键切状态”输入

## 15. 串口与蓝牙协议

### 15.1 分层

串口层分成两层：

- `Hardware/Serial.c`
  - 负责 `USART3` 初始化
  - 负责 TX 环形缓冲区
  - 负责 `USART3_IRQHandler`
  - 收到字节后喂给 `BlueSerial_InputByte()`
- `Hardware/BlueSerial.c`
  - 负责蓝牙协议缓冲
  - 负责按 `[...]` 括号包提取一帧
  - 负责字段切分和去空格

### 15.2 蓝牙解析规则

`BlueSerial` 的解析特征：

- 只识别 `[` 和 `]` 包住的帧
- 最大帧长：`128` 字节
- 最多字段数：`20`
- 单字段最大长度：`31`
- 命令名大小写不敏感
- 支持英文逗号 `,`
- 也兼容句点分隔命令
- 也兼容全角逗号/句号
- 但如果句点位于两个数字之间，会被保留为小数点

这意味着下面两类写法都能工作：

```text
[slider,P,0.10]
[slider.P.0.10]
```

### 15.3 上电后主动发送的帧

`AppTask_Init()` 在初始化蓝牙后会立即发送：

- `[STATE,...]`
- `[CFG,...]`
- `[MPU,...]`

### 15.4 周期遥测

默认遥测开关：

- `BOARD_BLUETOOTH_DEFAULT_STREAM_ENABLE = 0`

遥测周期：

- `BOARD_BLUETOOTH_TELEMETRY_MS = 100`

当前周期遥测只发**左前轮**三项：

```text
[plot,左前轮目标RPM,左前轮实际RPM,左前轮输出命令]
```

注意：

- 这里不是四轮全发
- 只发左前轮，方便曲线观察和在线调 PID

### 15.5 蓝牙命令总表

#### 查询/设置 PID

```text
[PID,GET]
[PID,SET,SPEED,kp,ki,kd]
[PID,LIMIT,SPEED,integralLimit,outputLimit]
```

返回格式：

```text
[PID,SPEED,kp,ki,kd,integralLimit,outputLimit]
```

#### 滑杆方式在线调参

```text
[slider,P,0.10]
[slider,I,0.05]
[slider,D,0.00]
[slider,T,200]
```

兼容别名：

- `P / KP`
- `I / KI`
- `D / KD`
- `T / TARGET`
- `slider` 也可写成 `s`

滑杆修改后不会立刻回包，而是延迟 `150ms` 后统一回复：

- `PID` 类修改：`[OK,SLIDER,PID_SPEED]` + PID 帧
- `T` 修改：`[OK,SLIDER,TARGET_SPEED]` + CFG 帧

#### 控制命令

```text
[CTRL,SPEED,200]
[CTRL,TURN,150]
[CTRL,HEADING,1]
[CTRL,PWM,700]
```

说明：

- `SPEED`：修改前进/后退目标速度幅值
- `TURN`：修改转向目标速度幅值
- `HEADING`：协议保留，当前尚未真正生效
- `PWM`：修改速度环输出限幅

#### 摇杆控制

```text
[joystick,0,0,x,y]
```

说明：

- `x / y` 范围都是 `-100 ~ 100`，按圆形摇杆范围理解
- `y` 负责前进/后退基础速度，`x` 负责差速转向修正
- `y = 0` 时默认停车，不进入原地旋转
- 斜方向表示“边走边转”，不会把速度和转向当成两个独立满量程通道
- 摇杆模式下左右轮最终目标速度都会受当前 `moveTargetRpm` 限制，不会超过手动设定的速度上限

#### 状态控制

```text
[STATE,GET]
[STATE,SET,FORWARD]
[STATE,SET,BACKWARD]
[STATE,SET,LEFT]
[STATE,SET,RIGHT]
[STATE,SET,STOP]
```

简写模式：

```text
[MODE,FORWARD]
[MODE,BACKWARD]
[MODE,LEFT]
[MODE,RIGHT]
[MODE,STOP]
```

状态帧格式：

```text
[STATE,当前状态,是否闭环,是否启用航向纠偏]
```

由于航向纠偏尚未实现，第三项当前通常是 `0`。

#### 遥测命令

```text
[TEL,GET]
[TEL,ON]
[TEL,OFF]
```

#### 配置查询

```text
[CFG,GET]
```

返回格式：

```text
[CFG,moveTargetRpm,turnTargetRpm,outputLimit,telemetryMs]
```

#### MPU/IMU 诊断

```text
[MPU,GET]
[MPU,ZERO]
[IMU,GET]
```

`[MPU,ZERO]` 会重新做陀螺 Z 轴零偏标定，期间有阻塞延时，因此发送时小车应静止。

`[MPU,GET]` 返回：

```text
[MPU,online,calibrated,gyroZDps,yawDeg,gyroBiasZDps]
```

`[IMU,GET]` 返回：

```text
[IMU,pitchAcc,rollAcc,pitchTilt,rollTilt,accNorm,accY,magFieldY,magHeadingRaw,magHeadingTiltComp,magHeadingFiltered,yawGyroOnly,yawFused,magCorrectionValid,accelTiltValid,magRecoveryPending]
```

## 16. OLED 显示逻辑

### 16.1 OLED 当前的用途

OLED 当前不是菜单界面，而是一个**实时诊断屏**。

`OLED_Task()` 每 `150ms` 刷新一次，内容分 8 行：

1. 当前整车状态 + 闭环标志
2. 速度环 `Kp Ki Kd`
3. 积分限幅和输出限幅
4. 左前轮：目标 / 实际 / 输出百分比
5. 右前轮：目标 / 实际 / 输出百分比
6. 左后轮：目标 / 实际 / 输出百分比
7. 右后轮：目标 / 实际 / 输出百分比
8. 电池电压 + 融合 yaw + Z 轴角速度

显示字体使用 `6x8`。

### 16.2 OLED 驱动能力

`Hardware/OLED.c` 不是只支持打印字符串，它已经带了完整图形库：

- 局部刷新 `OLED_UpdateArea`
- 清屏/局部清屏/反显
- ASCII 字符和字符串
- 数字、浮点、十六进制、二进制
- `OLED_Printf`
- 图片显示
- 点、线、矩形、三角形、圆、椭圆、圆弧

### 16.3 字库与中文

`OLED_Data.h` 当前启用了：

- `OLED_CHARSET_UTF8`

`OLED_Data.c` 里当前的 `OLED_CF16x16` 中文字模只内置了少量条目：

- 全角逗号 `，`
- 全角句号 `。`
- `你`
- `好`
- `世`
- `界`
- 一个默认“未找到字符”占位图案

所以：

- OLED 字符串函数支持 UTF-8 中文显示
- 但**不是完整中文库**
- 目前只是示例性内置了少量字模

### 16.4 OLED 驱动里需要特别注意的历史实现

这是整个工程里一个非常值得 AI 记住的细节：

- `OLED` 驱动已经实现了基于 `I2C1` 的硬件 I2C 写函数 `OLED_I2C_Write()`
- 但 `OLED_WriteCommand()` 在硬件 I2C 写成功后，又继续执行了一套旧的手工拉线式 `OLED_I2C_Start()/SendByte()/Stop()`
- 而这套旧路径里地址还写死成了 `0x78`

也就是说：

- 数据写入 `OLED_WriteData()` 走的是硬件 I2C 主路径
- 命令写入 `OLED_WriteCommand()` 里同时保留了旧的“软件 I2C/兼容路径”

这不一定立刻导致问题，但后续如果你要改 OLED 驱动，请优先意识到：

- 这里存在历史兼容代码残留
- 真正生效路径需要谨慎确认

## 17. I2C 总线管理

### 17.1 `BoardI2C.c`

`Hardware/BoardI2C.c` 是共用的 `I2C1` 板级访问层，供：

- `MPU6050`
- `HMC5883L`

使用。

它具备：

- 一次性初始化
- 忙标志等待
- 事件等待
- 错误标志检查
- 总线恢复
- 单字节写
- 单字节读
- 多字节读

### 17.2 与 OLED 的关系

虽然 OLED、MPU6050、HMC5883L 都在 `I2C1` 上，但代码上并不是完全统一走 `BoardI2C.c`：

- `MPU6050/HMC5883L` 走 `BoardI2C1_*`
- `OLED` 自己又实现了一套 I2C 访问逻辑

所以如果后续统一 I2C 驱动，这里是一个值得重构的点。

## 18. 电池采样

`Hardware/ADC_Bat.c` 使用：

- `ADC1`
- `PA5 / ADC_Channel_5`

当前逻辑：

- 12 位 ADC
- 单次转换
- `16` 点滑动平均
- 参考电压按 `3300mV`
- 分压恢复比例：`11:1`

对外提供：

- 原始 ADC 值
- `mV`
- `0.1V` 单位显示值

OLED 当前使用的是 `ADC_Bat_GetVoltageDeciV()`。

## 19. MPU6050、磁力计与姿态融合

### 19.1 `MPU6050.c`

当前 `MPU6050` 模块不只是“读陀螺 Z”，而是完整地做了：

- 加速度三轴读取
- 陀螺三轴读取
- 轴向交换/反向修正
- pitch/roll 的加速度估计
- 用陀螺积分预测姿态
- 用加速度做互补修正
- Z 轴零偏标定
- Z 轴动态微调零偏
- gyro-only yaw 积分
- fused yaw 存储

初始化流程中它还会：

1. 初始化共用 I2C
2. 初始化 `HMC5883L`
3. 读取 `WHO_AM_I`
4. 配置采样和量程
5. 先读一帧加速度初始化姿态
6. 先采一帧磁力计
7. 进行陀螺零偏标定
8. 初始化 `IMU_Fusion`

### 19.2 `HMC5883L.c`

当前磁力计模块具备：

- 设备 ID 检查
- 采样状态检查
- 原始三轴读取
- 轴向安装修正
- 硬铁/软铁补偿接口
- 在线采样时更新运行时 offset/scale
- 水平磁场强度有效性检查
- heading 计算

它支持三类校准来源：

- 板级静态硬铁偏移
- 板级静态软铁缩放
- 运行时在线 min/max 标定得到的 offset/scale

但是当前应用层**没有蓝牙命令去启动或结束 HMC5883L 标定**，这部分能力只在 C API 中保留：

- `HMC5883L_StartCalibration()`
- `HMC5883L_StopCalibration()`
- `HMC5883L_ResetCalibration()`

### 19.3 `IMU_Fusion.c`

`IMU_Fusion` 目前只关注 yaw 融合，不处理完整 9 轴姿态解算。

它做的事情包括：

- 读取 gyro-only yaw
- 读取磁航向
- 在允许时做倾斜补偿磁航向
- 低通平滑磁航向
- 用互补滤波修正 gyro yaw
- 当姿态不可信或扰动过大时，暂时暂停磁校正
- 条件恢复后重新锁定磁参考

因此：

- 这套 yaw 融合不是摆设
- 它已经能输出有意义的融合 yaw
- 但目前还没有真正回写到整车转向控制

## 20. Delay 与 SysTick

`System/Delay.c` 是一个阻塞式延时模块：

- `Delay_us`
- `Delay_ms`
- `Delay_s`

它直接用 `SysTick` 轮询计数，不依赖 `SysTick_Handler()`。  
因此当前工程的：

- `SysTick_Handler()` 是空的

这也是正常的，因为系统调度已经交给 `TIM6` 做了。

## 21. LED 模块

`Hardware/LED.c` 提供：

- `PA1` / `PA2` 两个低电平点亮的 LED
- 常亮、常灭、慢闪、快闪等模式

但当前主流程没有使用它，所以它属于“已实现、未接入当前业务”模块。

## 22. 当前关键配置参数

以下是最值得优先关注的配置：

### 22.1 周期与采样

| 宏 | 当前值 |
| --- | --- |
| `BOARD_ENCODER_SAMPLE_MS` | `20` |
| `BOARD_ENCODER_SPEED_AVG_SAMPLES` | `4` |
| `BOARD_PID_CONTROL_MS` | `20` |
| `BOARD_MPU6050_SAMPLE_MS` | `10` |
| `BOARD_HMC5883L_SAMPLE_MS` | `20` |
| `BOARD_BAT_SAMPLE_MS` | `50` |
| `BOARD_OLED_REFRESH_MS` | `150` |
| `BOARD_BLUETOOTH_TELEMETRY_MS` | `100` |

### 22.2 小车目标速度

| 宏 | 当前值 |
| --- | --- |
| `BOARD_CAR_MOVE_TARGET_SPEED_RPM` | `200` |
| `BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM` | `500` |
| `BOARD_CAR_TURN_TARGET_SPEED_RPM` | `150` |
| `BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM` | `200` |

### 22.3 速度环默认参数

| 宏 | 当前值 |
| --- | --- |
| `BOARD_SPEED_PID_KP` | `0.10f` |
| `BOARD_SPEED_PID_KI` | `0.05f` |
| `BOARD_SPEED_PID_KD` | `0.00f` |
| `BOARD_SPEED_PID_INTEGRAL_LIMIT` | `1200.0f` |
| `BOARD_SPEED_PID_OUTPUT_LIMIT` | `1000.0f` |

### 22.4 航向相关参数

虽然航向闭环还没接入，但以下参数已经在 `BoardConfig.h` 中定义：

- `BOARD_HEADING_PID_KP/KI/KD`
- `BOARD_HEADING_PID_INTEGRAL_LIMIT`
- `BOARD_HEADING_PID_OUTPUT_LIMIT_RPM`
- `BOARD_YAW_FUSION_ENABLE`
- `BOARD_YAW_COMPLEMENTARY_ALPHA`
- `BOARD_YAW_MAG_LPF_ALPHA`
- `BOARD_YAW_MAG_RECOVER_*`

这些参数目前主要服务于 IMU/磁航向融合，而不是整车闭环转向。

## 23. 当前已知现状与限制

### 23.1 航向纠偏尚未接入底盘闭环

这是当前最重要的“能力边界”。

- IMU 和磁力计都已经跑起来
- yaw 融合也已经跑起来
- 蓝牙里甚至有 `HEADING` 控制入口
- 但 `Motor.c` 还没有把这些量用于左右轮目标修正

### 23.2 遥测只看左前轮

`[plot,...]` 当前只回传：

- 左前轮目标
- 左前轮实际
- 左前轮输出

如果要做四轮一致性分析，需要扩展协议。

### 23.3 OLED 驱动有历史兼容路径残留

这一点前面已说明，后续修改 OLED 时务必注意。

### 23.4 项目中存在旧分析文档和旧说明文档

例如：

- `蓝牙调试说明.MD`
- `test.md`

这些文档有参考价值，但以当前源码为准。

### 23.5 工程文件中加入了大量标准库源文件

这不代表业务层都在使用它们。  
真正需要优先理解的是 `Core/App/System/Hardware/UserConfig` 这些自有代码。

## 24. 如果你是 AI，建议这样阅读这个工程

推荐阅读顺序：

1. `UserConfig/BoardConfig.h`
2. `Core/main.c`
3. `System/Timer.c`
4. `Hardware/Encoder.c`
5. `App/PID.c`
6. `Hardware/Motor.c`
7. `App/AppTask.c`
8. `Hardware/Serial.c`
9. `Hardware/BlueSerial.c`
10. `Hardware/ADC_Bat.c`
11. `Hardware/OLED.c`
12. `Hardware/BoardI2C.c`
13. `Hardware/MPU6050.c`
14. `Hardware/HMC5883L.c`
15. `Hardware/IMU_Fusion.c`

阅读时的核心判断原则：

- 先区分“已观测”与“已闭环使用”
- 先区分“协议已预留”与“功能已真实生效”
- 先看 `BoardConfig.h` 再改驱动
- 注意 `Motor` 是当前真正的整车业务中心
- 注意 `AppTask` 是当前所有人机交互和蓝牙协议的入口

## 25. 总结

当前工程已经具备一个很完整的“STM32F407 四轮小车底盘调参与诊断平台”的雏形：

- 底层：PWM、电机方向、编码器、ADC、I2C、USART 已齐全
- 控制层：四路速度闭环已跑通
- 观测层：OLED、本地按键、蓝牙协议、在线 PID 调参已可用
- 传感器层：MPU6050、HMC5883L、yaw 融合已具备
- 扩展层：航向辅助接口已预留，但尚未完成

如果后续继续开发，这个工程最自然的演进方向是：

1. 把航向辅助真正接入左右轮目标修正
2. 扩展蓝牙协议为四轮遥测
3. 清理 OLED 的历史兼容代码
4. 统一 `BoardI2C` 和 OLED 的 I2C 访问路径
5. 如有需要，再向更高层路径规划或姿态控制扩展
