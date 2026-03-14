# STM32 Embedded Journey 🚀  
STM32 嵌入式学习之旅

This repository documents my hands-on learning journey into embedded systems development using STM32 microcontrollers.

这个仓库记录了我使用 **STM32 微控制器进行嵌入式系统开发的学习过程**，包括实验代码、技术笔记以及实际项目实践。

The goal of this repository is to build a structured portfolio that demonstrates practical embedded engineering skills.

这个仓库的目标是构建一个系统化的技术作品集，用来展示真实的嵌入式开发能力。

---

# Repository Structure 仓库结构
```
STM32_Embedded_Journey
│
├── README.md
│
├── 01_Drivers
│    ├── inc
│    └── src
│
├── 02_Learning_Notes
│
├── 03_Tutorial_Labs
│
└── 04_Projects
```

## 01_Drivers
个人整理的 **底层驱动库**。
```
01_Drivers
├── inc
│    ├── oled.h
│    ├── mpu6050.h
│    └── w25q64.h
│
└── src
    ├── oled.c
    ├── mpu6050.c
    └── w25q64.c
```

这些驱动来自学习过程中的实验代码，并逐步进行封装整理。

---

## 02_Learning_Notes
技术学习笔记（Markdown 格式）。

示例：
```
02_Learning_Notes
├── STM32_Timer.md
├── PWM_Principle.md
├── ADC_DMA.md
└── I2C_vs_SPI.md
```

内容包括：
- 原理总结
- 外设配置流程
- 实验过程
- 调试经验
- 常见错误记录

---

## 03_Tutorial_Labs
跟随 STM32 教程完成的实验项目。
```
03_Tutorial_Labs
├── OLED_Display
├── Timer_Interrupt
├── PWM_LED
├── PWM_Servo
├── Encoder_Speed
├── ADC_Read
├── UART_Comm
└── MPU6050_Test
```

这些实验主要用于熟悉 **STM32 常用外设**：
- GPIO
- Timer
- PWM
- Encoder
- ADC
- DMA
- UART
- I2C

---

## 04_Projects
综合实践项目。
```
04_Projects
├── Smart_Inspection_Car
└── PID_Control_System
```

这些项目将多个 STM32 外设组合在一起，实现完整系统。

---

# Hardware 硬件平台
- **开发板**: STM32F103C8T6 / Blue Pill  
- **核心芯片**: ARM Cortex-M3

# Development Environment 开发环境
- **IDE**: Keil uVision 5  
- **下载器**: ST-Link  
- **编程语言**: C  
- **使用库**: STM32 Standard Peripheral Library

# Learning Progress 学习路线
- **Phase 1 — 基础外设**: GPIO, OLED, 基本输入输出  
- **Phase 2 — 定时与控制**: Timer, Interrupt, PWM, Encoder  
- **Phase 3 — 数据采集**: ADC, DMA, UART, 数据通信  
- **Phase 4 — 传感器与系统整合**: I2C, MPU6050, SPI Flash, 多模块系统整合

# Example Project 示例项目
**Smart Inspection Car**  
基于 STM32 的智能巡检小车系统，功能包括：
- 编码器测速
- PWM 电机驱动
- MPU6050 姿态检测
- OLED 实时数据显示
- 串口通信

# Learning Goals 学习目标
- 掌握 STM32 常用外设
- 理解嵌入式系统设计
- 完成多个实际项目
- 构建完整的嵌入式工程作品集

# Tools 工具
- **开发工具**: Keil uVision 5, STM32CubeMX, Git, GitHub Desktop  
- **操作系统**: Windows 11

# Continuous Learning 持续学习
This repository will continue to grow as new modules and projects are completed.  
随着学习的深入，这个仓库会不断增加新的模块和项目。  
The goal is to build a long-term embedded engineering portfolio.  
最终目标是构建一个长期维护的嵌入式工程作品集。