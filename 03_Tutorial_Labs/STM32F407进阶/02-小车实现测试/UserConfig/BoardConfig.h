#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "stm32f4xx.h"

/*
 * 板级统一配置说明
 * 1. 本文件集中放置引脚、外设和控制参数。
 * 2. 带单位的命名约定：
 *    _HZ   : 频率，单位 Hz
 *    _MS   : 时间，单位 ms
 *    _RPM  : 转速，单位 rpm（转/分钟）
 *    _MV   : 电压，单位 mV
 *    _DPS  : 角速度，单位 deg/s
 * 3. 标注“可调参数”的宏，都是后续调车时最常改的内容。
 * 4. 本次工程中 OLED 与 MPU6050 共用同一组 I2C1 总线，均走 PB8 / PB9。
 */

/* ==================== 时钟配置 ==================== */

/*
 * APB1 定时器输入时钟，必须与当前系统时钟树保持一致。
 * 本工程按 STM32F407 常见配置填写为 84 MHz。
 * 代码允许范围：1 ~ 168000000
 * 调节建议：只有在修改系统主频或 APB1 分频后才需要同步修改此值。
 */
#define BOARD_APB1_TIMER_CLOCK_HZ                84000000U

/* ==================== OLED / I2C 总线配置 ==================== */

/* OLED 与 MPU6050 共用 I2C1：PB8 -> SCL，PB9 -> SDA */
#define OLED_SCL_PORT                            GPIOB
#define OLED_SCL_PIN                             GPIO_Pin_8
#define OLED_SDA_PORT                            GPIOB
#define OLED_SDA_PIN                             GPIO_Pin_9
#define OLED_SCL_PINSOURCE                       GPIO_PinSource8
#define OLED_SDA_PINSOURCE                       GPIO_PinSource9
#define OLED_I2C_INSTANCE                        I2C1
#define OLED_I2C_GPIO_AF                         GPIO_AF_I2C1
#define OLED_I2C_RCC_APB1_PERIPH                 RCC_APB1Periph_I2C1

/*
 * OLED_I2C_ADDRESS:
 * OLED 的 8 位写地址，当前驱动按 StdPeriph 的写法使用 8 位地址。
 * 常见 SSD1306 模块地址为 0x78，部分模块可能是 0x7A。
 * 代码允许范围：0x00 ~ 0xFE，通常应为偶数地址。
 */
#define OLED_I2C_ADDRESS                         0x78U

/*
 * OLED_COLUMN_OFFSET:
 * 显存列偏移。SSD1306 常用 0，部分 SH1106 1.3 寸屏常用 2。
 * 代码允许范围：0 ~ 127
 * 建议调节范围：0 ~ 4
 * 现象说明：如果画面整体左右错位，可优先调这个参数。
 */
#define OLED_COLUMN_OFFSET                       0U

/*
 * OLED_POWER_ON_DELAY_MS:
 * OLED 上电后等待稳定的时间，防止刚上电就发送初始化命令。
 * 代码允许范围：0 ~ 65535
 * 建议调节范围：20 ~ 200 ms
 * 现象说明：若偶发初始化失败、冷启动花屏，可适当增大。
 */
#define OLED_POWER_ON_DELAY_MS                   100U

/*
 * OLED_I2C_CLOCK_SPEED:
 * OLED 与 MPU6050 共线后的总线速率。
 * 标准模式常用 100000，快速模式常用 400000。
 * 代码允许范围：1 ~ 400000
 * 建议调节范围：100000 ~ 400000
 * 现象说明：若线长较长或模块较多，先降到 100000 再排查。
 */
#define OLED_I2C_CLOCK_SPEED                     400000U

/*
 * OLED_I2C_TIMEOUT:
 * I2C 等待事件的超时计数，避免总线异常时程序卡死。
 * 代码允许范围：1 ~ 0xFFFFFFFF
 * 建议调节范围：1000 ~ 0xFFFF
 */
#define OLED_I2C_TIMEOUT                         0xFFFFU

/* ==================== MPU6050 配置 ==================== */

/*
 * MPU6050 复用 OLED 的 I2C1 总线。
 * 这里单独列出宏，方便后续模块直接引用，不必再关心 OLED 配置细节。
 */
#define BOARD_MPU6050_I2C_INSTANCE               OLED_I2C_INSTANCE
#define BOARD_MPU6050_I2C_GPIO_AF                OLED_I2C_GPIO_AF
#define BOARD_MPU6050_I2C_RCC_APB1_PERIPH        OLED_I2C_RCC_APB1_PERIPH
#define BOARD_MPU6050_SCL_PORT                   OLED_SCL_PORT
#define BOARD_MPU6050_SCL_PIN                    OLED_SCL_PIN
#define BOARD_MPU6050_SDA_PORT                   OLED_SDA_PORT
#define BOARD_MPU6050_SDA_PIN                    OLED_SDA_PIN
#define BOARD_MPU6050_SCL_PINSOURCE              OLED_SCL_PINSOURCE
#define BOARD_MPU6050_SDA_PINSOURCE              OLED_SDA_PINSOURCE

/*
 * BOARD_MPU6050_I2C_ADDRESS:
 * MPU6050 的 8 位写地址。
 * AD0 接地时通常为 0xD0，AD0 拉高时通常为 0xD2。
 * 代码允许范围：0x00 ~ 0xFE，通常应为偶数地址。
 */
#define BOARD_MPU6050_I2C_ADDRESS                0xD0U

/*
 * BOARD_MPU6050_WHO_AM_I_EXPECTED:
 * WHO_AM_I 寄存器期望值，MPU6050 常见为 0x68。
 */
#define BOARD_MPU6050_WHO_AM_I_EXPECTED          0x68U

/*
 * BOARD_MPU6050_SAMPLE_MS:
 * MPU6050 采样周期。
 * 代码允许范围：1 ~ 1000 ms
 * 建议调节范围：5 ~ 20 ms
 * 现象说明：值越小，航向响应越快；值越大，积分更平稳但纠偏会变钝。
 */
#define BOARD_MPU6050_SAMPLE_MS                  10U

/*
 * BOARD_MPU6050_CALIBRATION_SAMPLES:
 * 上电零漂校准的采样次数。
 * 标定时小车需要保持静止。
 * 代码允许范围：1 ~ 1000
 * 建议调节范围：100 ~ 400
 */
#define BOARD_MPU6050_CALIBRATION_SAMPLES        200U

/*
 * BOARD_MPU6050_CALIBRATION_DELAY_MS:
 * 零漂校准两次采样之间的等待时间。
 * 代码允许范围：0 ~ 100 ms
 * 建议调节范围：1 ~ 5 ms
 */
#define BOARD_MPU6050_CALIBRATION_DELAY_MS       2U

/*
 * BOARD_MPU6050_GYRO_SENSITIVITY_LSB:
 * 陀螺仪量程设为 ±250 dps 时，Z 轴角速度灵敏度约为 131 LSB/(deg/s)。
 * 如果后续修改了量程，这里也要同步修改。
 */
#define BOARD_MPU6050_GYRO_SENSITIVITY_LSB       131.0f

/*
 * BOARD_MPU6050_GYRO_LPF_ALPHA:
 * Z 轴角速度一阶低通滤波系数。
 * 取值范围：0.0f ~ 1.0f
 * 越接近 1 越相信新数据，越接近 0 越平滑。
 */
#define BOARD_MPU6050_GYRO_LPF_ALPHA             0.35f

/*
 * BOARD_MPU6050_GYRO_Z_REVERSE:
 * Z 轴方向补偿。
 * 0 表示当前方向正常；
 * 1 表示安装方向与软件定义相反，需要翻转。
 */
#define BOARD_MPU6050_GYRO_Z_REVERSE             0U

/* ==================== MPU6050 加速度计配置 ==================== */

/*
 * BOARD_MPU6050_ACCEL_CONFIG_VALUE:
 * 加速度计量程配置寄存器值。
 * 当前默认 0x00，对应 ±2g 量程。
 */
#define BOARD_MPU6050_ACCEL_CONFIG_VALUE         0x00U

/*
 * BOARD_MPU6050_ACCEL_SENSITIVITY_LSB:
 * 加速度计在 ±2g 量程下的灵敏度，单位 LSB/g。
 */
#define BOARD_MPU6050_ACCEL_SENSITIVITY_LSB      16384.0f

/*
 * 加速度计安装方向修正：
 * 先做 XY 交换，再做各轴反向，使输出统一对齐到车体坐标定义。
 */
#define BOARD_MPU6050_ACCEL_XY_SWAP              0U
#define BOARD_MPU6050_ACCEL_X_REVERSE            0U
#define BOARD_MPU6050_ACCEL_Y_REVERSE            0U
#define BOARD_MPU6050_ACCEL_Z_REVERSE            0U

/*
 * 陀螺仪 X/Y 轴方向默认与加速度计保持一致。
 * 如果后续确认 X/Y 轴角速度方向和倾角方向不一致，再单独微调这组宏。
 */
#define BOARD_MPU6050_GYRO_XY_SWAP               BOARD_MPU6050_ACCEL_XY_SWAP
#define BOARD_MPU6050_GYRO_X_REVERSE             BOARD_MPU6050_ACCEL_X_REVERSE
#define BOARD_MPU6050_GYRO_Y_REVERSE             BOARD_MPU6050_ACCEL_Y_REVERSE

/*
 * BOARD_MPU6050_ACCEL_NORM_MIN_G / MAX_G:
 * 只有当加速度模长接近 1g 时，才认为当前加速度姿态可用于倾斜补偿。
 */
#define BOARD_MPU6050_ACCEL_NORM_MIN_G           0.85f
#define BOARD_MPU6050_ACCEL_NORM_MAX_G           1.15f

/*
 * BOARD_MPU6050_TILT_CORRECTION_ALPHA:
 * roll / pitch 采用“陀螺预测 + 加速度慢校正”的互补滤波，
 * 该系数决定静稳时加速度校正的收敛速度。
 */
#define BOARD_MPU6050_TILT_CORRECTION_ALPHA      0.08f

/*
 * BOARD_MPU6050_TILT_CORRECTION_GYRO_DPS_THRESHOLD:
 * 只有当 X/Y 轴转动不剧烈时，才允许使用加速度去纠正倾角。
 * 这样可以避开“拿着传感器前端绕圈”时的线加速度污染。
 */
#define BOARD_MPU6050_TILT_CORRECTION_GYRO_DPS_THRESHOLD  18.0f

/* ==================== HMC5883L 配置 ==================== */

/*
 * HMC5883L 与 OLED / MPU6050 共用同一组 I2C1 总线。
 * 这里继续沿用板级统一的 I2C1 引脚和外设映射，避免重复初始化同一条总线。
 */
#define BOARD_HMC5883L_I2C_INSTANCE              OLED_I2C_INSTANCE
#define BOARD_HMC5883L_I2C_GPIO_AF               OLED_I2C_GPIO_AF
#define BOARD_HMC5883L_I2C_RCC_APB1_PERIPH       OLED_I2C_RCC_APB1_PERIPH
#define BOARD_HMC5883L_SCL_PORT                  OLED_SCL_PORT
#define BOARD_HMC5883L_SCL_PIN                   OLED_SCL_PIN
#define BOARD_HMC5883L_SDA_PORT                  OLED_SDA_PORT
#define BOARD_HMC5883L_SDA_PIN                   OLED_SDA_PIN
#define BOARD_HMC5883L_SCL_PINSOURCE             OLED_SCL_PINSOURCE
#define BOARD_HMC5883L_SDA_PINSOURCE             OLED_SDA_PINSOURCE

/*
 * BOARD_HMC5883L_I2C_ADDRESS:
 * HMC5883L 的 8 位写地址，7 位地址 0x1E 对应的写地址是 0x3C。
 */
#define BOARD_HMC5883L_I2C_ADDRESS               0x3CU

/*
 * BOARD_HMC5883L_SAMPLE_MS:
 * 磁力计建议以较低频率采样，减轻总线负担，同时与 15Hz / 30Hz 输出率匹配。
 */
#define BOARD_HMC5883L_SAMPLE_MS                 20U

/*
 * BOARD_HMC5883L_AVERAGE_SAMPLES:
 * 配置寄存器 A 的平均次数设置，可选 1 / 2 / 4 / 8。
 */
#define BOARD_HMC5883L_AVERAGE_SAMPLES           8U

/*
 * BOARD_HMC5883L_OUTPUT_RATE_HZ:
 * 配置寄存器 A 的输出数据率，常用 15Hz 或 30Hz。
 */
#define BOARD_HMC5883L_OUTPUT_RATE_HZ            15U

/*
 * BOARD_HMC5883L_GAIN_CONFIG:
 * 配置寄存器 B 的增益设置，高 3 位有效。
 * 默认 0x20 对应 1.3Ga 档位，和下方 1090 LSB/Ga 配套。
 */
#define BOARD_HMC5883L_GAIN_CONFIG               0x20U

/*
 * BOARD_HMC5883L_MODE_VALUE:
 * 模式寄存器设置，0x00 为连续测量模式。
 */
#define BOARD_HMC5883L_MODE_VALUE                0x00U

/*
 * BOARD_HMC5883L_GAIN_LSB_PER_GAUSS:
 * 当前增益配置下的灵敏度，用于把原始值换算为 Gauss。
 */
#define BOARD_HMC5883L_GAIN_LSB_PER_GAUSS        1090.0f

/*
 * BOARD_HMC5883L_HEADING_DECLINATION_DEG:
 * 当地磁偏角补偿，单位度。
 */
#define BOARD_HMC5883L_HEADING_DECLINATION_DEG   0.0f

/*
 * 安装方向修正：
 * XY 交换用于处理磁力计板子旋转 90 / 270 度安装的情况，
 * X/Y/Z 反向用于处理某个轴方向与车体坐标定义相反的情况。
 */
#define BOARD_HMC5883L_XY_SWAP                   0U
#define BOARD_HMC5883L_X_REVERSE                 0U
#define BOARD_HMC5883L_Y_REVERSE                 0U
#define BOARD_HMC5883L_Z_REVERSE                 0U

/*
 * 硬铁偏置，单位保持与原始磁场计数一致。
 * 如果已经离线标定过，可以把标定得到的偏置直接填在这里。
 */
#define BOARD_HMC5883L_HARD_IRON_OFFSET_X        0.0f
#define BOARD_HMC5883L_HARD_IRON_OFFSET_Y        0.0f
#define BOARD_HMC5883L_HARD_IRON_OFFSET_Z        0.0f

/*
 * 软铁缩放系数，默认全为 1.0f。
 * 如果已经离线标定过，可以把每个轴的缩放系数直接填在这里。
 */
#define BOARD_HMC5883L_SOFT_IRON_SCALE_X         1.0f
#define BOARD_HMC5883L_SOFT_IRON_SCALE_Y         1.0f
#define BOARD_HMC5883L_SOFT_IRON_SCALE_Z         1.0f

/*
 * BOARD_HMC5883L_MIN_VALID_FIELD_GAUSS:
 * 二维水平磁场模长过小时，不参与航向计算，避免坏数据硬融合。
 */
#define BOARD_HMC5883L_MIN_VALID_FIELD_GAUSS     0.02f

/*
 * BOARD_HMC5883L_CALIBRATION_MIN_SPAN_RAW:
 * 在线校准时，每个轴至少要扫到这么大的半径，才认为校准结果有效。
 */
#define BOARD_HMC5883L_CALIBRATION_MIN_SPAN_RAW  80.0f

/* ==================== 航向融合参数 ==================== */

/*
 * BOARD_YAW_FUSION_ENABLE:
 * 1 表示启用磁力计 + 陀螺的互补滤波融合；
 * 0 表示只保留陀螺积分输出，便于对比调试。
 */
#define BOARD_YAW_FUSION_ENABLE                  1U

/*
 * BOARD_YAW_COMPLEMENTARY_ALPHA:
 * 互补滤波中陀螺预测项的权重，越接近 1 越相信短时陀螺响应。
 */
#define BOARD_YAW_COMPLEMENTARY_ALPHA            0.98f

/*
 * BOARD_YAW_STATIC_GYRO_DPS_THRESHOLD:
 * 当去偏后的角速度绝对值小于该阈值时，允许缓慢重估陀螺零偏。
 */
#define BOARD_YAW_STATIC_GYRO_DPS_THRESHOLD      1.2f

/*
 * BOARD_YAW_MAG_LPF_ALPHA:
 * 磁航向低通系数，越小越稳，越大响应越快。
 */
#define BOARD_YAW_MAG_LPF_ALPHA                  0.25f

/*
 * BOARD_YAW_TILT_COMPENSATION_ENABLE:
 * 1 表示启用基于加速度计姿态的倾斜补偿磁航向。
 */
#define BOARD_YAW_TILT_COMPENSATION_ENABLE       1U

/*
 * BOARD_YAW_MAG_MAX_TILT_DEG:
 * 当 pitch / roll 超过该角度时，本次磁航向不参与融合校正。
 */
#define BOARD_YAW_MAG_MAX_TILT_DEG               35.0f

/*
 * BOARD_YAW_ACCEL_TILT_LPF_ALPHA:
 * 加速度计姿态角低通系数，越大跟随越快，越小越平稳。
 */
#define BOARD_YAW_ACCEL_TILT_LPF_ALPHA           0.25f

/*
 * BOARD_YAW_MAG_HEADING_USE_TILT_COMP:
 * 1 表示主用倾斜补偿后的磁航向；
 * 0 表示保留二维磁航向，便于回退对比。
 */
#define BOARD_YAW_MAG_HEADING_USE_TILT_COMP      1U

/*
 * BOARD_YAW_MAG_REJECT_ON_BAD_ACCEL:
 * 1 表示当加速度模长异常时，拒绝使用本次倾斜补偿磁航向。
 */
#define BOARD_YAW_MAG_REJECT_ON_BAD_ACCEL        1U

/*
 * BOARD_YAW_MAG_TRUST_WHEN_STATIC_ONLY:
 * 1 表示只有角速度很小时才允许磁航向参与校正；
 * 0 表示运动中也允许参与，但仍受倾角和加速度模长门限约束。
 */
#define BOARD_YAW_MAG_TRUST_WHEN_STATIC_ONLY     0U

/*
 * BOARD_YAW_MAG_STABLE_GYRO_DPS_THRESHOLD:
 * 扰动恢复时判断“已经基本放稳”的角速度阈值。
 * 只有小于该阈值并持续一段时间，才重新锁定磁参考。
 */
#define BOARD_YAW_MAG_STABLE_GYRO_DPS_THRESHOLD  6.0f

/*
 * BOARD_YAW_MAG_RECOVER_MAX_TILT_DEG:
 * 进入扰动恢复模式后，只有回到接近平放的倾角范围，才允许重新接回磁参考。
 * 对平面小车建议设得比普通磁航向可用倾角更严格。
 */
#define BOARD_YAW_MAG_RECOVER_MAX_TILT_DEG       12.0f

/*
 * BOARD_YAW_MAG_RECOVER_STABLE_MS:
 * 从“扰动状态”回到“稳定状态”后，需要连续稳定这么久，才恢复磁航向校正。
 */
#define BOARD_YAW_MAG_RECOVER_STABLE_MS          300U

/*
 * BOARD_YAW_MAG_RECOVER_GAIN:
 * 扰动恢复阶段对 yaw 误差使用更大的校正增益，加快回到绝对航向。
 */
#define BOARD_YAW_MAG_RECOVER_GAIN               0.18f

/*
 * BOARD_YAW_MAG_RECOVER_EXIT_ERROR_DEG:
 * 当恢复阶段的 yaw 误差已经小于该阈值时，退出增强恢复模式。
 */
#define BOARD_YAW_MAG_RECOVER_EXIT_ERROR_DEG     2.5f

/*
 * BOARD_YAW_GYRO_REBIAS_ENABLE:
 * 1 表示启用静止时陀螺零偏在线微调；
 * 0 表示仅保留上电零偏校准。
 */
#define BOARD_YAW_GYRO_REBIAS_ENABLE             1U

/*
 * BOARD_YAW_GYRO_REBIAS_ALPHA:
 * 陀螺零偏在线微调系数，建议非常小，避免把正常慢转误吸收到 bias。
 */
#define BOARD_YAW_GYRO_REBIAS_ALPHA              0.002f

/* ==================== 蓝牙串口配置 ==================== */

/* 蓝牙固定使用 USART3：PD8 -> TX，PD9 -> RX */
#define BOARD_BLUETOOTH_USART                    USART3
#define BOARD_BLUETOOTH_USART_RCC                RCC_APB1Periph_USART3
#define BOARD_BLUETOOTH_GPIO_RCC                 RCC_AHB1Periph_GPIOD
#define BOARD_BLUETOOTH_GPIO_PORT                GPIOD
#define BOARD_BLUETOOTH_TX_PIN                   GPIO_Pin_8
#define BOARD_BLUETOOTH_RX_PIN                   GPIO_Pin_9
#define BOARD_BLUETOOTH_TX_PINSOURCE             GPIO_PinSource8
#define BOARD_BLUETOOTH_RX_PINSOURCE             GPIO_PinSource9
#define BOARD_BLUETOOTH_GPIO_AF                  GPIO_AF_USART3
#define BOARD_BLUETOOTH_IRQ                      USART3_IRQn

/*
 * BOARD_BLUETOOTH_BAUDRATE:
 * 蓝牙串口波特率。
 * 为兼容常见 HC-05/HC-06 默认设置，这里保留为 9600。
 * 如果你的蓝牙模块已经改过波特率，可同步调整。
 */
#define BOARD_BLUETOOTH_BAUDRATE                 9600U

/*
 * BOARD_BLUETOOTH_TELEMETRY_MS:
 * 蓝牙连续回传调试数据的时间间隔。
 * 代码允许范围：10 ~ 1000 ms
 * 建议调节范围：50 ~ 200 ms
 */
#define BOARD_BLUETOOTH_TELEMETRY_MS             100U

/*
 * BOARD_BLUETOOTH_DEFAULT_STREAM_ENABLE:
 * 上电后是否默认开启连续遥测。
 * 0：关闭，等蓝牙命令打开
 * 1：默认开启
 */
#define BOARD_BLUETOOTH_DEFAULT_STREAM_ENABLE    0U

/* ==================== PWM 配置 ==================== */

/* TIM4 的 4 路 PWM 输出引脚：PD12 / PD13 / PD14 / PD15 */
#define BOARD_PWM_GPIO_RCC                       RCC_AHB1Periph_GPIOD
#define BOARD_PWM_GPIO_PORT                      GPIOD
#define BOARD_PWM_GPIO_AF                        GPIO_AF_TIM4
#define BOARD_PWM_TIMER_RCC                      RCC_APB1Periph_TIM4
#define BOARD_PWM_TIMER                          TIM4

/*
 * BOARD_PWM_PRESCALER:
 * PWM 定时器预分频值，实际分频系数 = BOARD_PWM_PRESCALER + 1。
 */
#define BOARD_PWM_PRESCALER                      0U

/*
 * BOARD_PWM_PERIOD:
 * PWM 自动重装值 ARR，决定 PWM 周期和分辨率。
 * 当前配置下频率约为 84MHz / (1 * 8400) = 10kHz。
 */
#define BOARD_PWM_PERIOD                         8399U

/*
 * BOARD_PWM_MAX_DUTY:
 * 上层软件使用的“占空比满量程”。
 * PWM_SetDuty() 会把 0 ~ BOARD_PWM_MAX_DUTY 线性映射到 CCR。
 */
#define BOARD_PWM_MAX_DUTY                       1000U

#define BOARD_PWM_A_PIN                          GPIO_Pin_12
#define BOARD_PWM_B_PIN                          GPIO_Pin_13
#define BOARD_PWM_C_PIN                          GPIO_Pin_14
#define BOARD_PWM_D_PIN                          GPIO_Pin_15

#define BOARD_PWM_A_PINSOURCE                    GPIO_PinSource12
#define BOARD_PWM_B_PINSOURCE                    GPIO_PinSource13
#define BOARD_PWM_C_PINSOURCE                    GPIO_PinSource14
#define BOARD_PWM_D_PINSOURCE                    GPIO_PinSource15

/* ==================== 电机方向引脚 ==================== */

#define BOARD_MOTOR_A_IN1_PORT                   GPIOE
#define BOARD_MOTOR_A_IN1_PIN                    GPIO_Pin_8
#define BOARD_MOTOR_A_IN2_PORT                   GPIOE
#define BOARD_MOTOR_A_IN2_PIN                    GPIO_Pin_9

#define BOARD_MOTOR_B_IN1_PORT                   GPIOE
#define BOARD_MOTOR_B_IN1_PIN                    GPIO_Pin_10
#define BOARD_MOTOR_B_IN2_PORT                   GPIOD
#define BOARD_MOTOR_B_IN2_PIN                    GPIO_Pin_0

#define BOARD_MOTOR_C_IN1_PORT                   GPIOE
#define BOARD_MOTOR_C_IN1_PIN                    GPIO_Pin_12
#define BOARD_MOTOR_C_IN2_PORT                   GPIOE
#define BOARD_MOTOR_C_IN2_PIN                    GPIO_Pin_13

#define BOARD_MOTOR_D_IN1_PORT                   GPIOE
#define BOARD_MOTOR_D_IN1_PIN                    GPIO_Pin_14
#define BOARD_MOTOR_D_IN2_PORT                   GPIOE
#define BOARD_MOTOR_D_IN2_PIN                    GPIO_Pin_15

/* 电机驱动待机使能 STBY */
#define BOARD_MOTOR_STBY_GPIO_RCC                RCC_AHB1Periph_GPIOB
#define BOARD_MOTOR_STBY_GPIO_PORT               GPIOB
#define BOARD_MOTOR_STBY_PIN                     GPIO_Pin_5

/* ==================== 按键配置 ==================== */

#define BOARD_KEY_GPIO_RCC                       RCC_AHB1Periph_GPIOD
#define BOARD_KEY_GPIO_PORT                      GPIOD
#define BOARD_KEY_PIN                            GPIO_Pin_3

/*
 * BOARD_KEY_DEBOUNCE_MS:
 * 按键消抖时间。Key_Scan() 只有在按键状态稳定持续到这个时间后才认定按下。
 * 代码允许范围：1 ~ 65535
 * 建议调节范围：10 ~ 50 ms
 * 现象说明：过小会误触发，过大会感觉按键反应变慢。
 */
#define BOARD_KEY_DEBOUNCE_MS                    20U

/* ==================== 系统节拍定时器 ==================== */

#define BOARD_TICK_TIMER_RCC                     RCC_APB1Periph_TIM6
#define BOARD_TICK_TIMER                         TIM6
#define BOARD_TICK_IRQ                           TIM6_DAC_IRQn

/* ==================== 任务调度周期 ==================== */

/*
 * 以下任务周期都作为 1ms 节拍下的倒计时初值使用，因此不能为 0。
 */

/*
 * BOARD_ENCODER_SAMPLE_MS:
 * 编码器测速采样周期，越小响应越快，越大读数越稳。
 */
#define BOARD_ENCODER_SAMPLE_MS                  20U

/*
 * BOARD_ENCODER_SPEED_AVG_SAMPLES:
 * 速度估算的滑动平均窗口长度。
 * 更新节拍仍然是 20ms，但会对多次采样结果做平均，以提高速度显示和速度闭环反馈的稳定性。
 * 数值越大，速度数据越平滑，但响应也会更慢。
 */
#define BOARD_ENCODER_SPEED_AVG_SAMPLES          5U

/*
 * BOARD_ENCODER_COUNTS_PER_OUTPUT_REV:
 * 输出轴每转一圈对应的编码器总计数。
 * 计算方式：编码器原始 PPR × 四倍频系数 × 电机减速比。
 * 当前配置为：13 × 4 × 20 = 1040。
 */
#define BOARD_ENCODER_PPR                        13U
#define BOARD_ENCODER_QUADRATURE_MULTIPLIER      4U
#define BOARD_MOTOR_GEAR_RATIO                   20U
#define BOARD_ENCODER_COUNTS_PER_OUTPUT_REV      (BOARD_ENCODER_PPR * BOARD_ENCODER_QUADRATURE_MULTIPLIER * BOARD_MOTOR_GEAR_RATIO)

/*
 * BOARD_PID_CONTROL_MS:
 * 闭环 PID 控制周期。
 * 调节建议：通常不应大于编码器采样周期太多，当前与编码器同为 20ms。
 */
#define BOARD_PID_CONTROL_MS                     20U

/*
 * BOARD_BAT_SAMPLE_MS:
 * 电池电压采样周期。
 */
#define BOARD_BAT_SAMPLE_MS                      50U

/*
 * BOARD_OLED_REFRESH_MS:
 * OLED 刷新周期。
 */
#define BOARD_OLED_REFRESH_MS                    150U

/* ==================== 小车运动参数 ==================== */

/*
 * BOARD_CAR_MOVE_TARGET_SPEED_RPM:
 * 小车前进 / 后退时的基础目标速度，单位为 RPM（转/分钟）。
 * 该值是速度环的默认设定值，可通过蓝牙在运行时调整。
 */
#define BOARD_CAR_MOVE_TARGET_SPEED_RPM          200

/*
 * BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM:
 * 蓝牙修改基础目标速度时的上限保护。
 * 建议不超过编码器与电机在当前减速比下的稳定测速范围。
 */
#define BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM      500

/*
 * BOARD_CAR_TURN_TARGET_SPEED_RPM:
 * 左转 / 右转时左右轮差速控制的默认目标速度，单位为 RPM（转/分钟）。
 * 本次改造后，转向同样走速度闭环，不再走纯 PWM 直推。
 */
#define BOARD_CAR_TURN_TARGET_SPEED_RPM          150

/*
 * 兼容旧代码的别名。
 * 如果工程里仍有旧的 BOARD_CAR_TURN_SPEED 用法，会自动映射到新的转向目标速度。
 */
#define BOARD_CAR_TURN_SPEED                     BOARD_CAR_TURN_TARGET_SPEED_RPM

/*
 * BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM:
 * 蓝牙修改转向目标速度时的上限保护。
 */
#define BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM      200

/* ==================== 航向纠偏参数 ==================== */

/*
 * BOARD_HEADING_PID_KP / KI / KD:
 * 航向外环 PID 默认参数。
 * 这个外环的输出单位是“左右轮速度补偿量”，单位为 RPM（转/分钟）。
 * 建议先从小 Kp 起步，Ki 和 Kd 保守设置。
 */
#define BOARD_HEADING_PID_KP                     0.58f
#define BOARD_HEADING_PID_KI                     0.0012f
#define BOARD_HEADING_PID_KD                     0.086f

/*
 * BOARD_HEADING_PID_INTEGRAL_LIMIT:
 * 航向外环积分限幅。
 */
#define BOARD_HEADING_PID_INTEGRAL_LIMIT         300.0f

/*
 * BOARD_HEADING_PID_OUTPUT_LIMIT_RPM:
 * 航向纠偏量限幅，限制左右轮目标速度的最大补偿幅度，单位为 RPM（转/分钟）。
 * 建议不超过基础速度的 1/2。
 */
#define BOARD_HEADING_PID_OUTPUT_LIMIT_RPM       14.5f

/* ==================== 速度环 PID 默认参数 ==================== */

/*
 * BOARD_SPEED_PID_KP / KI / KD:
 * 四轮速度环 PID 默认参数。
 */
#define BOARD_SPEED_PID_KP                       0.10f
#define BOARD_SPEED_PID_KI                       0.05f
#define BOARD_SPEED_PID_KD                       0.00f

/*
 * BOARD_SPEED_PID_INTEGRAL_LIMIT:
 * 速度环积分项限幅。
 */
#define BOARD_SPEED_PID_INTEGRAL_LIMIT           1200.0f

/*
 * BOARD_SPEED_PID_OUTPUT_LIMIT:
 * 速度环总输出限幅。
 * 当前配置让 PID 输出直接受 PWM 最大值保护。
 * 这个值也会作为“PWM 强度”运行时调参的默认上限。
 */
#define BOARD_SPEED_PID_OUTPUT_LIMIT             ((float)BOARD_PWM_MAX_DUTY)

/* ==================== 轮子与编码器编号映射 ==================== */

/*
 * 下面 4 个宏用于定义“左前 / 左后 / 右前 / 右后”对应的电机编号。
 * 如果硬件接线顺序改了，优先改这里，而不是到业务代码里到处改。
 */
#define BOARD_CAR_LEFT_FRONT_MOTOR               0U
#define BOARD_CAR_RIGHT_FRONT_MOTOR              1U
#define BOARD_CAR_LEFT_REAR_MOTOR                2U
#define BOARD_CAR_RIGHT_REAR_MOTOR               3U

/*
 * 编码器与逻辑车轮的映射。
 * 若电机和编码器安装顺序不一致，只需调整这里的映射关系。
 */
#define BOARD_CAR_LEFT_FRONT_ENCODER             BOARD_CAR_LEFT_FRONT_MOTOR
#define BOARD_CAR_RIGHT_FRONT_ENCODER            BOARD_CAR_RIGHT_FRONT_MOTOR
#define BOARD_CAR_LEFT_REAR_ENCODER              BOARD_CAR_LEFT_REAR_MOTOR
#define BOARD_CAR_RIGHT_REAR_ENCODER             BOARD_CAR_RIGHT_REAR_MOTOR

/* ==================== 电机方向补偿 ==================== */

/*
 * 电机反向补偿：
 * 0 表示当前方向正常
 * 1 表示该电机方向与软件定义相反，需要翻转
 */
#define BOARD_MOTOR_A_REVERSE                    0U
#define BOARD_MOTOR_B_REVERSE                    0U
#define BOARD_MOTOR_C_REVERSE                    0U
#define BOARD_MOTOR_D_REVERSE                    0U

/* ==================== 编码器方向补偿 ==================== */

/*
 * 编码器反向补偿：
 * 0 表示当前计数方向正常
 * 1 表示计数方向相反，需要翻转
 */
#define BOARD_ENCODER_A_REVERSE                  0U
#define BOARD_ENCODER_B_REVERSE                  1U
#define BOARD_ENCODER_C_REVERSE                  0U
#define BOARD_ENCODER_D_REVERSE                  1U

/* ==================== 编码器滤波与计数中心 ==================== */

/*
 * BOARD_ENCODER_IC_FILTER:
 * 定时器输入捕获数字滤波等级。
 * 数值越大，抗抖动越强，但响应也会变慢。
 */
#define BOARD_ENCODER_IC_FILTER                  6U

/*
 * 以下宏是编码器定时器的自动重装值和居中值。
 * 一般不要改，除非你明确修改了定时器位宽处理方式。
 */
#define BOARD_ENCODER_16BIT_PERIOD               0xFFFFU
#define BOARD_ENCODER_32BIT_PERIOD               0xFFFFFFFFUL
#define BOARD_ENCODER_16BIT_CENTER               0x7FFFU
#define BOARD_ENCODER_32BIT_CENTER               0x7FFFFFFFUL

/* ==================== 编码器 1：TIM8，PC6 / PC7 ==================== */

#define BOARD_ENCODER_A_TIMER                    TIM8
#define BOARD_ENCODER_A_TIMER_RCC                RCC_APB2Periph_TIM8
#define BOARD_ENCODER_A_IS_APB2                  1U
#define BOARD_ENCODER_A_IS_32BIT                 0U
#define BOARD_ENCODER_A_GPIO_RCC_A               RCC_AHB1Periph_GPIOC
#define BOARD_ENCODER_A_GPIO_RCC_B               RCC_AHB1Periph_GPIOC
#define BOARD_ENCODER_A_PORT_A                   GPIOC
#define BOARD_ENCODER_A_PIN_A                    GPIO_Pin_6
#define BOARD_ENCODER_A_PINSOURCE_A              GPIO_PinSource6
#define BOARD_ENCODER_A_PORT_B                   GPIOC
#define BOARD_ENCODER_A_PIN_B                    GPIO_Pin_7
#define BOARD_ENCODER_A_PINSOURCE_B              GPIO_PinSource7
#define BOARD_ENCODER_A_GPIO_AF                  GPIO_AF_TIM8

/* ==================== 编码器 2：TIM5，PA0 / PA1 ==================== */

#define BOARD_ENCODER_B_TIMER                    TIM5
#define BOARD_ENCODER_B_TIMER_RCC                RCC_APB1Periph_TIM5
#define BOARD_ENCODER_B_IS_APB2                  0U
#define BOARD_ENCODER_B_IS_32BIT                 1U
#define BOARD_ENCODER_B_GPIO_RCC_A               RCC_AHB1Periph_GPIOA
#define BOARD_ENCODER_B_GPIO_RCC_B               RCC_AHB1Periph_GPIOA
#define BOARD_ENCODER_B_PORT_A                   GPIOA
#define BOARD_ENCODER_B_PIN_A                    GPIO_Pin_0
#define BOARD_ENCODER_B_PINSOURCE_A              GPIO_PinSource0
#define BOARD_ENCODER_B_PORT_B                   GPIOA
#define BOARD_ENCODER_B_PIN_B                    GPIO_Pin_1
#define BOARD_ENCODER_B_PINSOURCE_B              GPIO_PinSource1
#define BOARD_ENCODER_B_GPIO_AF                  GPIO_AF_TIM5

/* ==================== 编码器 3：TIM3，PA6 / PA7 ==================== */

#define BOARD_ENCODER_C_TIMER                    TIM3
#define BOARD_ENCODER_C_TIMER_RCC                RCC_APB1Periph_TIM3
#define BOARD_ENCODER_C_IS_APB2                  0U
#define BOARD_ENCODER_C_IS_32BIT                 0U
#define BOARD_ENCODER_C_GPIO_RCC_A               RCC_AHB1Periph_GPIOA
#define BOARD_ENCODER_C_GPIO_RCC_B               RCC_AHB1Periph_GPIOA
#define BOARD_ENCODER_C_PORT_A                   GPIOA
#define BOARD_ENCODER_C_PIN_A                    GPIO_Pin_6
#define BOARD_ENCODER_C_PINSOURCE_A              GPIO_PinSource6
#define BOARD_ENCODER_C_PORT_B                   GPIOA
#define BOARD_ENCODER_C_PIN_B                    GPIO_Pin_7
#define BOARD_ENCODER_C_PINSOURCE_B              GPIO_PinSource7
#define BOARD_ENCODER_C_GPIO_AF                  GPIO_AF_TIM3

/* ==================== 编码器 4：TIM1，PA8 / PE11 ==================== */

#define BOARD_ENCODER_D_TIMER                    TIM1
#define BOARD_ENCODER_D_TIMER_RCC                RCC_APB2Periph_TIM1
#define BOARD_ENCODER_D_IS_APB2                  1U
#define BOARD_ENCODER_D_IS_32BIT                 0U
#define BOARD_ENCODER_D_GPIO_RCC_A               RCC_AHB1Periph_GPIOA
#define BOARD_ENCODER_D_GPIO_RCC_B               RCC_AHB1Periph_GPIOE
#define BOARD_ENCODER_D_PORT_A                   GPIOA
#define BOARD_ENCODER_D_PIN_A                    GPIO_Pin_8
#define BOARD_ENCODER_D_PINSOURCE_A              GPIO_PinSource8
#define BOARD_ENCODER_D_PORT_B                   GPIOE
#define BOARD_ENCODER_D_PIN_B                    GPIO_Pin_11
#define BOARD_ENCODER_D_PINSOURCE_B              GPIO_PinSource11
#define BOARD_ENCODER_D_GPIO_AF                  GPIO_AF_TIM1

/* ==================== 电池 ADC 配置 ==================== */

/* 电池电压采样引脚：PA5 -> ADC1_IN5 */
#define BOARD_BAT_ADC_GPIO_RCC                   RCC_AHB1Periph_GPIOA
#define BOARD_BAT_ADC_PORT                       GPIOA
#define BOARD_BAT_ADC_PIN                        GPIO_Pin_5
#define BOARD_BAT_ADC_CHANNEL                    ADC_Channel_5
#define BOARD_BAT_ADC_RCC                        RCC_APB2Periph_ADC1
#define BOARD_BAT_ADC                            ADC1
#define BOARD_BAT_ADC_SAMPLE_TIME                ADC_SampleTime_144Cycles

/*
 * BOARD_BAT_ADC_FILTER_SIZE:
 * 电池电压滑动平均滤波长度。
 * 注意 g_batSampleIndex 是 uint8_t，因此该值不应超过 255。
 */
#define BOARD_BAT_ADC_FILTER_SIZE                16U

/*
 * BOARD_ADC_REFERENCE_MV:
 * ADC 参考电压，单位 mV。
 */
#define BOARD_ADC_REFERENCE_MV                   3300U

/*
 * BOARD_BAT_DIVIDER_NUMERATOR / DENOMINATOR:
 * 电池分压还原比例。
 * 例如外部分压把电池电压缩小到 1/4，则这里应设为 4 / 1。
 */
#define BOARD_BAT_DIVIDER_NUMERATOR              11U
#define BOARD_BAT_DIVIDER_DENOMINATOR            1U

/*
 * BOARD_BAT_VOLTAGE_OFFSET_MV:
 * 电池电压测量偏移补偿，单位 mV。
 */
#define BOARD_BAT_VOLTAGE_OFFSET_MV              0U

#endif
