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
# 单独模拟电平和控制器标志，验证实际共用总线恢复代码；不连接实物、不运行厂商HAL。
add_executable(i2c2_recovery_test i2c2_recovery_test.c
    "${F407_ROOT}/User/Hardware/I2C2_Bus.c")
target_include_directories(i2c2_recovery_test PRIVATE stubs "${F407_ROOT}/User/Hardware")
target_compile_options(i2c2_recovery_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME i2c2_recovery COMMAND i2c2_recovery_test)
set_tests_properties(i2c2_recovery PROPERTIES TIMEOUT 5)
# 这个自检启动单独的子进程，确认失败保留非零状态和位置日志，且无需关闭任何弹窗。
add_test(NAME touch_driver_failure_reporting
    COMMAND "${CMAKE_COMMAND}"
        "-DTOUCH_TEST_EXE=$<TARGET_FILE:touch_driver_test>"
        -P "${CMAKE_CURRENT_LIST_DIR}/verify_failure_exit.cmake")
set_tests_properties(touch_driver_failure_reporting PROPERTIES TIMEOUT 10)
