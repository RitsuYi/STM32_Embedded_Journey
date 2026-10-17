cmake_minimum_required(VERSION 3.22)
project(touch_driver_test C)
set(CMAKE_C_STANDARD 11)
get_filename_component(F407_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# 编译真实驱动，用下面的HAL/RTOS替身模拟芯片数据和锁，避免测试依赖板卡。
add_executable(touch_driver_test touch_driver_test.c
    "${F407_ROOT}/User/Hardware/FT6336U.c"
    "${F407_ROOT}/User/Hardware/I2C2_Bus.c")
target_include_directories(touch_driver_test PRIVATE stubs "${F407_ROOT}/User/Hardware")
target_compile_options(touch_driver_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
enable_testing()
add_test(NAME touch_driver COMMAND touch_driver_test)
