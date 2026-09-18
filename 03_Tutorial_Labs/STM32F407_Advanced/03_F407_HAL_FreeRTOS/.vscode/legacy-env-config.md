# 两台设备切换操作手册

目标：仓库中的配置文件只修改一次。以后在两台设备之间同步代码时，不再修改仓库里的
绝对路径，只让每台设备使用自己的 Windows 环境变量。

## 一、仓库配置只需修改一次

按下面内容修改 5 个文件。完成并提交后，两台设备同步这套配置即可。

### 1. 修改 `.vscode/tasks.json`

找到 `Configure`、`Build`、`Clean` 三个任务，将三处：

```json
"command": "cmake"
```

全部替换为：

```json
"command": "${env:CMAKE_ROOT}/bin/cmake.exe"
```

找到 `CubeMX` 任务，将：

```json
"command": "D:/Program Files/STMicroelectronics/STM32Cube/STM32CubeMX/STM32CubeMX.exe"
```

替换为：

```json
"command": "${env:STM32CubeMX_PATH}/STM32CubeMX.exe"
```

其他内容不需要修改。

### 2. 修改 `.vscode/settings.json`

将整个文件改成：

```json
{
    "cmake.cmakePath": "${env:CMAKE_ROOT}/bin/cmake.exe",
    "cmake.useCMakePresets": "always",
    "cmake.configureOnOpen": true,
    "cmake.configurePreset": "debug-armgcc",
    "cmake.buildPreset": "build-debug",
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build/Debug/compile_commands.json",
    "C_Cpp.default.compilerPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-gcc.exe",
    "C_Cpp.default.intelliSenseMode": "gcc-arm",
    "cortex-debug.armToolchainPath": "${env:ARM_GCC_ROOT}/bin",
    "cortex-debug.gdbPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-gdb.exe",
    "cortex-debug.objdumpPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-objdump.exe",
    "cortex-debug.JLinkGDBServerPath.windows": "${env:JLINK_ROOT}/JLinkGDBServerCL.exe"
}
```

### 3. 修改 `.vscode/launch.json`

只替换以下三项：

```json
"serverpath": "${env:JLINK_ROOT}/JLinkGDBServerCL.exe",
"gdbPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-gdb.exe",
"objdumpPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-objdump.exe"
```

其他调试参数不需要修改。

### 4. 修改 `CMakePresets.json`

将 `armgcc-base` 预设改成下面这样：

```json
{
    "name": "armgcc-base",
    "hidden": true,
    "generator": "Ninja",
    "toolchainFile": "${sourceDir}/cmake/gcc-arm-none-eabi.cmake",
    "environment": {
        "PATH": "$env{ARM_GCC_ROOT}/bin;$penv{PATH}"
    },
    "cacheVariables": {
        "CMAKE_MAKE_PROGRAM": "$env{NINJA_ROOT}/ninja.exe",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
    }
}
```

`debug-armgcc`、`release-armgcc` 和两个构建预设不需要修改。

### 5. 修改 `.vscode/scripts/flash.cmd`

将：

```bat
set "JLINK_EXE=D:\Program Files\JLink_V958\JLink.exe"
```

替换为：

```bat
set "JLINK_EXE=%JLINK_ROOT%\JLink.exe"
```

## 二、切换到设备二：当前独立工具链设备

### 第 1 步：设置设备二环境变量

在普通 PowerShell 中执行：

```powershell
setx CMAKE_ROOT "D:\DevEnv\cmake-4.2.3-windows-x86_64"
setx ARM_GCC_ROOT "D:\DevEnv\10 2021.10"
setx NINJA_ROOT "D:\DevEnv\ninja"
setx JLINK_ROOT "D:\Program Files\JLink_V958"
setx STM32CubeMX_PATH "D:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX"
```

这些路径已于 2026-09-18 在当前设备验证存在。

### 第 2 步：重启 VS Code

完全关闭所有 VS Code 窗口，然后重新打开工程。`setx` 设置的变量不会自动进入已经
运行的 VS Code 进程，因此这一步不能省略。

### 第 3 步：检查路径

在重新打开的 VS Code 终端中执行：

```powershell
Test-Path "$env:CMAKE_ROOT/bin/cmake.exe"
Test-Path "$env:ARM_GCC_ROOT/bin/arm-none-eabi-gcc.exe"
Test-Path "$env:ARM_GCC_ROOT/bin/arm-none-eabi-gdb.exe"
Test-Path "$env:NINJA_ROOT/ninja.exe"
Test-Path "$env:JLINK_ROOT/JLinkGDBServerCL.exe"
Test-Path "$env:JLINK_ROOT/JLink.exe"
Test-Path "$env:STM32CubeMX_PATH/STM32CubeMX.exe"
```

七项都应返回 `True`。

### 第 4 步：配置并编译

在 VS Code 中依次运行：

1. `Terminal -> Run Task -> Configure`
2. `Terminal -> Run Task -> Build`

也可以直接按 `Ctrl+Shift+B`。`Build` 会先调用 `Configure`。

## 三、切换到设备一：STM32CubeCLT 设备

### 第 1 步：设置设备一环境变量

在设备一的普通 PowerShell 中执行：

```powershell
setx CMAKE_ROOT "D:\ST\STM32CubeCLT_1.18.0\CMake"
setx ARM_GCC_ROOT "D:\ST\STM32CubeCLT_1.18.0\GNU-tools-for-STM32"
setx NINJA_ROOT "D:\ST\STM32CubeCLT_1.18.0\Ninja\bin"
setx JLINK_ROOT "D:\Program Files\JLink_V958"
setx STM32CubeMX_PATH "D:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX"
```

其中 GNU Arm 路径来自设备一原有配置；CMake 和 Ninja 路径需要在设备一第一次操作时
确认一次。如果设备一的实际安装目录不同，就只修改该设备上的环境变量值，不要修改
仓库配置。

### 第 2 步：确认 CubeCLT 路径

在设备一执行：

```powershell
Test-Path "$env:CMAKE_ROOT/bin/cmake.exe"
Test-Path "$env:ARM_GCC_ROOT/bin/arm-none-eabi-gcc.exe"
Test-Path "$env:ARM_GCC_ROOT/bin/arm-none-eabi-gdb.exe"
Test-Path "$env:NINJA_ROOT/ninja.exe"
```

四项都应返回 `True`。如果某项返回 `False`，在 CubeCLT 目录内查找对应文件：

```powershell
Get-ChildItem "D:\ST\STM32CubeCLT_1.18.0" -Recurse -File |
    Where-Object Name -In cmake.exe,ninja.exe,arm-none-eabi-gcc.exe
```

根据搜索结果调整该设备的 `CMAKE_ROOT`、`ARM_GCC_ROOT` 或 `NINJA_ROOT`，使上面的
`Test-Path` 全部返回 `True`。

### 第 3 步：确认 J-Link 和 CubeMX 路径

执行：

```powershell
Test-Path "$env:JLINK_ROOT/JLinkGDBServerCL.exe"
Test-Path "$env:JLINK_ROOT/JLink.exe"
Test-Path "$env:STM32CubeMX_PATH/STM32CubeMX.exe"
```

如果设备一的 J-Link 位于：

```text
D:\Program Files\SEGGER\JLink_V958
```

只需在设备一重新设置：

```powershell
setx JLINK_ROOT "D:\Program Files\SEGGER\JLink_V958"
```

### 第 4 步：重启 VS Code 并编译

完全关闭 VS Code，重新打开工程，然后依次运行 `Configure` 和 `Build`。

## 四、以后两台设备互相切换时

完成上面的一次性配置后，每次换设备只执行以下步骤：

1. 拉取或同步仓库代码。
2. 不修改 `.vscode` 和 `CMakePresets.json` 中的路径。
3. 确认当前设备的环境变量仍然存在。
4. 完全重新启动 VS Code。
5. 运行 `Configure`，再运行 `Build`。

`Configure` 任务已经带有 `--fresh`，会清除 CMake 缓存中来自另一台设备的绝对路径。
不要在两台设备之间同步 `build/` 目录。

## 五、快速判断当前使用的是哪台设备配置

在 VS Code 终端执行：

```powershell
$env:CMAKE_ROOT
$env:ARM_GCC_ROOT
$env:NINJA_ROOT
```

- 输出以 `D:\DevEnv` 开头：当前使用设备二的独立工具链。
- 输出以 `D:\ST\STM32CubeCLT_1.18.0` 开头：当前使用设备一的 CubeCLT。
