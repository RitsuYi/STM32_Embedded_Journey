@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BUILD_DIR=%~dp0..\..\build\Debug"
set "JLINK_EXE=%JLINK_ROOT%\JLink.exe"

if not exist "%JLINK_EXE%" (
    echo ERROR: J-Link Commander was not found:
    echo   %JLINK_EXE%
    exit /b 1
)

if not exist "%BUILD_DIR%\" (
    echo ERROR: Debug build directory was not found:
    echo   %BUILD_DIR%
    echo Run the Build task first.
    exit /b 1
)

set "ELF_FILE=%BUILD_DIR%\03_F407_HAL_FreeRTOS.elf"
if not exist "%ELF_FILE%" (
    echo ERROR: Firmware ELF file was not found:
    echo   %ELF_FILE%
    echo Run the Build task first.
    exit /b 1
)

set "COMMAND_FILE=%TEMP%\stm32f407_flash_%RANDOM%_%RANDOM%.jlink"
> "%COMMAND_FILE%" (
    echo device STM32F407VG
    echo si SWD
    echo speed 4000
    echo reset
    echo loadfile "%ELF_FILE%"
    echo reset
    echo go
    echo exit
)

echo Flashing:
echo   %ELF_FILE%
"%JLINK_EXE%" -NoGui 1 -ExitOnError 1 -CommandFile "%COMMAND_FILE%"
set "JLINK_STATUS=%ERRORLEVEL%"

if exist "%COMMAND_FILE%" del "%COMMAND_FILE%"

if not "%JLINK_STATUS%"=="0" (
    echo ERROR: J-Link Commander failed with exit code %JLINK_STATUS%.
    exit /b %JLINK_STATUS%
)

echo Flash completed successfully.
exit /b 0
