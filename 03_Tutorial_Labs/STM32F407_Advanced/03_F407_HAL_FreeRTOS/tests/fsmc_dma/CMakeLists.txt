cmake_minimum_required(VERSION 3.22)
project(fsmc_dma_test C)
set(CMAKE_C_STANDARD 11)
# 编译真实驱动，仅用HAL/RTOS状态模型替代硬件，验证DMA源寿命和异常收尾。
add_executable(fsmc_dma_test fsmc_dma_test.c ../../User/Hardware/ST7796_FSMC.c)
target_include_directories(fsmc_dma_test PRIVATE stubs ../../User/Hardware)
target_compile_options(fsmc_dma_test PRIVATE -Wall -Wextra -Werror
    -include "${CMAKE_CURRENT_SOURCE_DIR}/stubs/test_platform.h")
enable_testing()
add_test(NAME fsmc_dma_contract COMMAND fsmc_dma_test)
