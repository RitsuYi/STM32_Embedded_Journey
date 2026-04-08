# PID 小车直线行驶左轮慢于右轮修复方案

## 1. 问题描述

当前现象是：

- 小车在“目标直线行驶”状态下，左轮速度明显小于右轮
- 车身无法保持直线，实际会走出弧线
- 继续单纯调 `Kp / Ki / Kd` 往往只能改善平均速度，不能真正消除左右轮差速

结合仓库中的 PID 旧版本代码分析，这个问题不是单一参数问题，而是“控制结构 + 资源冲突 + 可能的编码器/硬件差异”共同导致的。

## 2. 先给结论

按当前仓库中的 PID 版本逻辑，左轮慢于右轮并不奇怪，原因主要有 5 个：

1. 当前只有“平均速度 PID”，没有“左右轮平衡控制”。
2. `DifSpeed` 被算出来了，但没有真正用于闭环修正。
3. `TIM1` 同时被拿来做 PWM 和定时中断，这个设计本身就不稳。
4. `PA9` 同时被 PWM 和 `USART1_TX` 共用，存在明显引脚冲突。
5. 左右编码器配置不完全对称，且当前 PID 本身缺少积分限幅和更完整的抗饱和处理。

所以，正确修复顺序不是“直接猛调 PID”，而应该是：

1. 先排除编码器和机械差异问题。
2. 再修正定时器和引脚冲突。
3. 最后把控制结构升级成“速度环 + 平衡环”或“双轮独立速度环”。

## 3. 现有代码中的关键问题

### 3.1 只有平均速度环，没有左右轮平衡环

在 `User/main.c` 旧 PID 代码中：

- `AveSpeed = (LeftSpeed + RightSpeed) / 2.0f;`
- `DifSpeed = LeftSpeed - RightSpeed;`
- `SpeedPID` 只对 `AveSpeed` 做控制
- `DifPWM` 没有根据 `DifSpeed` 计算

也就是说，系统虽然知道“左右轮速度不同”，但没有任何闭环动作去纠正这个差异。

现有逻辑本质上是：

```c
AvePWM = SpeedPID.Out;
LeftPWM  = AvePWM + DifPWM;
RightPWM = AvePWM - DifPWM;
```

但你当前代码里在直行时：

```c
DifPWM = 0;
```

所以左右轮实际上拿到的是同一份平均控制量。只要左电机摩擦更大、减速箱更紧、轮胎更涩、驱动压降更高，左轮就必然比右轮慢。

### 3.2 `TIM1` 被重复用作 PWM 和控制定时器

当前仓库中：

- `Hardware/PWM.c` 用 `TIM1` 输出 PWM
- `SYSTEM/Timer.c` 也用 `TIM1` 产生更新中断

这会导致：

- PWM 的时基被 `Timer_Init()` 改写
- 你以为自己在输出 `0~100` 的占空比，实际可能已经不是原来的比例
- PID 环的采样基准和电机 PWM 基准绑死在一个定时器上，后续很难稳定调试

这个问题必须优先修。

### 3.3 `PA9` 存在引脚冲突

当前仓库中：

- `Hardware/PWM.c` 使用 `PA8 / PA9` 作为 `TIM1_CH1 / TIM1_CH2`
- `Hardware/Serial.c` 使用 `PA9 / PA10` 作为 `USART1_TX / USART1_RX`

因此在 PID 旧版 `main()` 里如果同时调用：

- `Motor_Init();`
- `Serial_Init();`

那么 `PA9` 就被右电机 PWM 和串口发送同时占用。这种冲突会直接影响右轮控制稳定性，必须消除。

### 3.4 左右编码器配置不完全对称

`Hardware/Encoder.c` 中：

- 左编码器 `TIM2` 使用 `Rising / Rising`
- 右编码器 `TIM3` 使用 `Falling / Rising`

这不一定绝对错误，但说明左右轮的编码器方向定义和极性并不完全一致，必须通过实验确认：

- 同向前进时，左右计数是否同号
- 同一 PWM 下，左右计数是否同量级
- 是否存在某一路计数偏小、抖动大、方向反了的问题

### 3.5 PID 结构太基础

当前 `PID.c` 只有最基础的位置式 PID：

- 没有积分限幅
- 没有积分分离
- 没有饱和后的抗积分累积
- 没有输出斜坡限制

这在小车低速启动、左右电机不一致、负载变化时，比较容易出现：

- 一边起不来
- 一边冲得过快
- 长时间偏差后积分堆积

## 4. 修复目标

本次修复的目标建议定义为：

1. 直线命令下，左右轮实际速度差控制在 5% 以内。
2. 2 米直线行驶时，车身明显偏航消失或显著减小。
3. 电池电压下降时，左右轮仍能维持基本同步。
4. 后续可以继续扩展到蓝牙调参或循迹控制，而不是一次性的“补偿常数工程”。

## 5. 推荐修复路径

建议按下面顺序做，顺序不要乱。

## 5.1 第一阶段：先确认是“真慢”还是“测错”

先不要上 PID，先做开环测试。

### 测试方法

1. 把车轮悬空，避免地面摩擦影响。
2. 暂时不要初始化 `Serial_Init()`，避免 `PA9` 冲突。
3. 暂时不要启用速度环，只给左右轮相同 PWM，例如 `20 / 30 / 40 / 50`。
4. 每隔 `100ms` 读取一次左右编码器计数，连续记录 20 次。
5. 分别记录：
   - `LeftCount`
   - `RightCount`
   - `LeftCount / RightCount`

### 判断标准

- 如果左右计数都为正，说明方向定义至少基本一致。
- 如果一边为正、一边为负，说明编码器方向定义有问题。
- 如果相同 PWM 下某一边长期小很多，例如差距超过 `10%~15%`，优先怀疑：
  - 电机/减速箱机械差异
  - 编码器极性或接线
  - 电机驱动通道差异

### 建议做的交叉验证

1. 左右电机插头对调一次，看“慢”的现象是否跟着电机走。
2. 左右编码器接口对调一次，看“慢”的现象是否跟着编码器数据走。
3. 左右轮胎、轮毂、轴承、底盘摩擦阻力做手动检查。

结论判断：

- 如果“慢”跟着电机走，优先处理机械差异或做软件偏置补偿。
- 如果“慢”跟着编码器数据走，优先处理编码器极性、接线、计数配置。
- 如果“慢”固定在左侧不变，优先检查左侧驱动桥、电源走线、焊接和底盘阻力。

## 5.2 第二阶段：先修结构问题，再谈 PID

这一步属于必须做。

### 方案 A：把控制中断从 `TIM1` 挪走

推荐把控制定时器改为 `TIM4`，让 `TIM1` 专职输出 PWM。

推荐原因：

- `TIM1` 当前已经在 `Hardware/PWM.c` 中用作双路 PWM
- `TIM2 / TIM3` 已经被编码器占用
- `TIM4` 适合做 `1ms` 周期控制节拍

### 推荐修改点

修改 `SYSTEM/Timer.c`：

- 把 `RCC_APB2Periph_TIM1` 改为 `RCC_APB1Periph_TIM4`
- 把 `TIM1` 改为 `TIM4`
- 把中断号 `TIM1_UP_IRQn` 改为 `TIM4_IRQn`
- 把中断函数改名为 `TIM4_IRQHandler()`

建议目标：

- 中断周期保持 `1ms`
- 速度环每 `10ms` 执行一次

示意代码：

```c
void Timer_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_InternalClockConfig(TIM4);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);

    TIM_ClearFlag(TIM4, TIM_FLAG_Update);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM4, ENABLE);
}
```

### 方案 B：停用 `USART1`，不要再占 `PA9`

在 PID 版本里，最短路径是：

- 不要调用 `Serial_Init()`
- 调试输出统一走 `BlueSerial` 或暂时关闭串口打印

如果确实需要保留串口调试，建议：

- 改用不与 PWM 冲突的串口方案
- 或做引脚重映射

但对当前工程来说，最简单稳妥的做法就是：

**PID 调试阶段先完全停用 `USART1`**

## 5.3 第三阶段：校正编码器方向和量程

这一步非常关键。

### 必做检查

在电机同向前进时：

- `Encoder_Get(1)` 和 `Encoder_Get(2)` 都应该是正数
- 两边数值应该在同一量级

在电机同向后退时：

- 两边计数都应该是负数

如果不是这样，先不要调 PID。

### 对 `TIM3` 的建议

当前 `TIM3` 用的是：

```c
TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12,
                           TIM_ICPolarity_Falling,
                           TIM_ICPolarity_Rising);
```

建议做一次统一性验证：

1. 先尝试把 `TIM3` 也改成 `Rising / Rising`
2. 测试正转和反转时的计数正负是否正确
3. 如果方向反了，再通过交换编码器 A/B 相或在软件里取负修正

注意：

- 这里不是说“一定要改成 `Rising / Rising`”
- 而是要确保左右轮的计数方向定义、比例关系和实际运动一致

## 5.4 第四阶段：控制结构升级

这里给你两种修法。

### 方案 1：最小改动版，增加“平衡环”

这是基于你当前代码最容易落地的修法。

核心思想：

- 保留原有的平均速度环 `SpeedPID`
- 再新增一个左右轮平衡环 `BalancePID`
- 让 `BalancePID` 专门把 `DifSpeed = LeftSpeed - RightSpeed` 拉回 0

### 控制关系

```c
AveSpeed = (LeftSpeed + RightSpeed) * 0.5f;
DifSpeed = LeftSpeed - RightSpeed;

SpeedPID.Target   = TargetSpeed;
SpeedPID.Actual   = AveSpeed;
PID_Update(&SpeedPID);
AvePWM = SpeedPID.Out;

BalancePID.Target = 0;
BalancePID.Actual = DifSpeed;
PID_Update(&BalancePID);
DifPWM = BalancePID.Out;

LeftPWM  = AvePWM + DifPWM + LeftBias;
RightPWM = AvePWM - DifPWM + RightBias;
```

这样当左轮慢于右轮时：

- `DifSpeed = LeftSpeed - RightSpeed` 会变成负数
- `BalancePID` 输出会朝着“增大左轮、减小右轮”的方向调节
- 直线能力会明显提升

### 这个方案的优点

- 改动小
- 能复用你当前的 `AvePWM / DifPWM` 结构
- 适合先把车跑直

### 这个方案的缺点

- 左右轮仍然不是完全独立控制
- 一旦负载变化较大，长期效果不如双轮独立环

### 方案 2：推荐长期方案，左右轮独立速度 PID

这是更标准的智能车方案。

核心思想：

- 左轮一个速度 PID
- 右轮一个速度 PID
- 目标直行时，两边目标速度相同
- 目标转向时，两边目标速度不同

控制形式如下：

```c
LeftPID.Target  = TargetSpeed + TurnSpeed;
RightPID.Target = TargetSpeed - TurnSpeed;

LeftPID.Actual  = LeftSpeed;
RightPID.Actual = RightSpeed;

PID_Update(&LeftPID);
PID_Update(&RightPID);

LeftPWM  = LeftPID.Out  + LeftBias;
RightPWM = RightPID.Out + RightBias;
```

### 这个方案的优点

- 最符合左右轮本体差异的实际情况
- 每个轮子都能单独补偿摩擦、死区、负载变化
- 后面接循迹、蓝牙遥控、姿态修正都更方便

### 缺点

- 改动比“平衡环方案”稍大

### 我的建议

如果你想“尽快修好”，先上“速度环 + 平衡环”。

如果你想“后面少返工”，直接改成“双轮独立速度环”。

## 5.5 第五阶段：加入偏置补偿和死区补偿

就算用了 PID，小车两边也往往存在启动死区不同的问题。

比如：

- 左电机 `PWM < 18` 几乎不动
- 右电机 `PWM < 14` 就能转

如果不处理这个死区差异，那么低速时左轮仍会长期慢于右轮。

### 推荐做法

先测出左右轮的最小启动 PWM：

- `LeftStartPWM`
- `RightStartPWM`

然后在 PID 输出后做补偿：

```c
static int16_t ApplyDeadZone(int16_t pwm, int16_t start_pwm)
{
    if (pwm > 0) return pwm + start_pwm;
    if (pwm < 0) return pwm - start_pwm;
    return 0;
}
```

最终输出：

```c
LeftPWM  = ApplyDeadZone(LeftPWM,  LeftStartPWM);
RightPWM = ApplyDeadZone(RightPWM, RightStartPWM);
```

如果左轮始终偏慢，还可以增加固定偏置：

```c
#define LEFT_BIAS_PWM   2
#define RIGHT_BIAS_PWM  0
```

注意：

- 固定偏置只能作为辅助修正
- 不能代替真正的左右轮闭环控制

## 5.6 第六阶段：扩展 PID 结构，避免积分问题

建议升级 `PID_t`，增加以下字段：

- `IntegralMax`
- `IntegralMin`
- `LastOut`

建议升级 `PID_Update()`，至少补上：

1. 积分限幅
2. 输出限幅
3. 输出饱和时的抗积分累积

示意代码：

```c
void PID_Update(PID_t *p)
{
    p->Error1 = p->Error0;
    p->Error0 = p->Target - p->Actual;

    p->ErrorInt += p->Error0;

    if (p->ErrorInt > p->IntegralMax) p->ErrorInt = p->IntegralMax;
    if (p->ErrorInt < p->IntegralMin) p->ErrorInt = p->IntegralMin;

    p->Out = p->Kp * p->Error0
           + p->Ki * p->ErrorInt
           + p->Kd * (p->Error0 - p->Error1);

    if (p->Out > p->OutMax) p->Out = p->OutMax;
    if (p->Out < p->OutMin) p->Out = p->OutMin;
}
```

如果你采用“双轮独立 PID”，这个升级很有价值。

## 6. 推荐调参顺序

调参请按下面顺序来，不要跳步。

### 步骤 1：先做开环偏置测试

目标：

- 找到左右轮各自的启动 PWM
- 找到相同 PWM 下的自然速度差

输出结果建议记录成表格：

| PWM | LeftCount | RightCount | Ratio |
| --- | --- | --- | --- |
| 20 |  |  |  |
| 30 |  |  |  |
| 40 |  |  |  |
| 50 |  |  |  |

### 步骤 2：先只开 P，不开 I、不加 D

先把积分和微分都关掉：

- `Ki = 0`
- `Kd = 0`

目的：

- 先看系统方向是否正确
- 防止一上来积分堆积，把现象搞乱

### 步骤 3：先调“平衡环”或左右轮独立的 P

这一步先只解决“左慢右快”的问题。

建议观察：

- 左右轮差值是否快速收敛
- 是否出现左右来回摆动

现象判断：

- 如果差值缩小很慢，适当加大 `Kp`
- 如果两轮来回抢速度，说明 `Kp` 偏大

### 步骤 4：再加速度环

当左右轮同步性基本正常后，再调平均速度。

顺序建议：

1. 先调速度环 `Kp`
2. 再少量加入 `Ki`
3. `Kd` 最后再考虑，很多小车速度环甚至可以先不用 `Kd`

### 步骤 5：最后再加积分

积分的作用是消除稳态误差，但它最容易掩盖结构问题。

所以只有在下面都做完以后再加 `Ki`：

- 编码器方向正确
- 定时器冲突已消除
- 引脚冲突已消除
- 左右轮平衡控制已建立

## 7. 最短可执行修复方案

如果你现在时间有限，只想先把车跑直，建议按这个最短路径来：

1. 把 `SYSTEM/Timer.c` 的控制定时器从 `TIM1` 改到 `TIM4`
2. 在 PID 版本中停用 `Serial_Init()`，避免 `PA9` 冲突
3. 校验 `Encoder_Get(1)` 和 `Encoder_Get(2)` 同向是否同号
4. 增加 `BalancePID`
5. 用 `DifSpeed` 计算 `DifPWM`
6. 给左轮增加小的固定偏置 `LEFT_BIAS_PWM`
7. 先只用 `P` 调平衡，再逐步加入 `I`

这是最省改动、见效最快的办法。

## 8. 推荐的文件修改清单

### 必改

- `SYSTEM/Timer.c`
  - 控制定时器从 `TIM1` 改到 `TIM4`
- `User/main.c`
  - 恢复 PID 版主循环
  - 新增 `BalancePID` 或 `LeftPID / RightPID`
  - 不再让 `DifPWM` 固定为 0
- `Hardware/Serial.c`
  - PID 阶段停用 `USART1`
- `Hardware/Encoder.c`
  - 重新确认左右编码器极性、方向、计数量级

### 强烈建议修改

- `User/PID.h`
  - 增加积分限幅字段
- `User/PID.c`
  - 增加抗积分饱和处理
- `Hardware/Motor.c`
  - 统一加入限幅和死区补偿入口

## 9. 验收标准

建议把“修好”定义成下面几个标准，而不是只看主观感觉：

1. 悬空测试时，相同 PWM 下左右编码器计数误差小于 `5%~8%`
2. 地面直线测试时，1 米内没有明显单侧偏转
3. 低速起步时左右轮都能稳定启动，不出现一边先冲、一边不动
4. 电池电量下降后，仍然能保持基本直线
5. 连续运行 30 秒后，左右轮速度差不会越来越大

## 10. 不建议的修法

下面这些做法不建议单独使用：

- 只调大 `SpeedPID.Kp`
- 只给左轮硬加很大的常数补偿
- 在 `TIM1` 冲突和 `PA9` 冲突都没解决前就开始细调 PID
- 编码器方向还没确认就开始加积分

因为这些做法大概率只能“看起来短暂变好”，后面会反复出问题。

## 11. 我的最终建议

如果你要一个稳定、可继续扩展的版本，优先级建议如下：

1. 先把控制中断移到 `TIM4`
2. 停用 `USART1`，解除 `PA9` 冲突
3. 校验左右编码器方向和计数对称性
4. 先上“速度环 + 平衡环”
5. 跑通后再升级成“双轮独立速度环”
6. 最后再做死区补偿和蓝牙在线调参

一句话总结：

**你现在的问题，本质不是“某个 PID 参数不对”，而是当前控制结构没有真正闭环约束左右轮同步。先把硬件资源冲突和左右轮平衡环补上，再调 PID，效果会立刻不一样。**
