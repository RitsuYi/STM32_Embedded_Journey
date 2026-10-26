cmake_minimum_required(VERSION 3.22)
project(gui_migration_test C CXX)
set(CMAKE_C_STANDARD 11)
get_filename_component(F407_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${F407_ROOT}/lv_conf.h" CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${F407_ROOT}/LVGL" lvgl)
target_include_directories(lvgl PUBLIC "${F407_ROOT}")
file(GLOB_RECURSE APP_SOURCES
    "${F407_ROOT}/User/GUI/measurement/*.c"
    "${F407_ROOT}/User/GUI/instrument/*.c"
    "${F407_ROOT}/User/GUI/fonts/*.c"
    "${F407_ROOT}/User/GUI/assets/*.c") # 现有页面已依赖纹理/素材，主机也需链接同一实现。
add_executable(gui_migration_test gui_migration_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/gui_demo.c" "${F407_ROOT}/User/GUI/gui_app.c")
target_include_directories(gui_migration_test PRIVATE "${F407_ROOT}/User/GUI")
target_compile_options(gui_migration_test PRIVATE -Wall -Wextra -Wformat=2)
if(MINGW)
    # MinGW采用C99格式化实现，正确支持旧回归打印size_t使用的%zu。
    target_compile_definitions(gui_migration_test PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()
target_link_libraries(gui_migration_test PRIVATE lvgl m)
enable_testing()
add_test(NAME gui_migration COMMAND gui_migration_test)

# H6.5回归：通过实际输入适配和LVGL命中检测点击按钮，不直接伪造CLICKED事件。
add_executable(gui_touch_test gui_touch_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/gui_demo.c" "${F407_ROOT}/User/GUI/lvgl_port.c")
target_include_directories(gui_touch_test PRIVATE touch_stubs stubs
    "${F407_ROOT}/User/GUI" "${F407_ROOT}/User/Hardware")
target_compile_options(gui_touch_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(gui_touch_test PRIVATE lvgl m)
add_test(NAME gui_touch COMMAND gui_touch_test)

# DMA流水线验证运行真实LVGL与适配层；硬件在Wait时才读取源数据以检查寿命。
add_executable(gui_dma_test gui_dma_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/lvgl_port.c" "${F407_ROOT}/User/GUI/gui_demo.c"
    "${F407_ROOT}/User/GUI/gui_app.c")
target_include_directories(gui_dma_test PRIVATE touch_stubs stubs
    "${F407_ROOT}/User/GUI" "${F407_ROOT}/User/Hardware")
target_compile_options(gui_dma_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(gui_dma_test PRIVATE lvgl m)
add_test(NAME gui_dma_pipeline COMMAND gui_dma_test)

# 系统信息专项运行真实页面/字体/LVGL，后端与寄存器模型另测，不依赖开发板。
add_executable(system_info_test ../system_info/system_info_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/gui_demo.c")
target_include_directories(system_info_test PRIVATE "${F407_ROOT}/User/GUI")
target_compile_options(system_info_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(system_info_test PRIVATE lvgl m)
add_test(NAME system_info_ui COMMAND system_info_test)
add_executable(system_monitor_test ../system_info/system_monitor_test.c
    "${F407_ROOT}/User/Task/System_Monitor.c" "${F407_ROOT}/User/Hardware/Runtime_Clock.c")
target_include_directories(system_monitor_test PRIVATE ../system_info/stubs
    "${F407_ROOT}/User/Task" "${F407_ROOT}/User/Hardware")
target_compile_options(system_monitor_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME system_monitor_math COMMAND system_monitor_test)

# 运行真实手势与转场，覆盖上下回弹、快慢惯性、返回连点和中断后重开。
add_executable(gui_motion_test gui_motion_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/gui_demo.c")
target_include_directories(gui_motion_test PRIVATE "${F407_ROOT}/User/GUI")
target_compile_options(gui_motion_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(gui_motion_test PRIVATE lvgl m)
add_test(NAME gui_motion COMMAND gui_motion_test)
